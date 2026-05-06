/***************************************************************************
 * Copyright 1996-2024 Synopsys, Inc.
 *
 * This Synopsys software and all associated documentation are proprietary
 * to Synopsys, Inc. and may only be used pursuant to the terms and
 * conditions of a written license agreement with Synopsys, Inc.
 * All other use, reproduction, modification, or distribution of the
 * Synopsys software or the associated documentation is strictly prohibited.
 *
 * =========================================================================
 * XSPI Unified Controller – Header
 * Supports PIO mode (Section 4.4.2) and CDMA/ACMD mode (Section 4.4.1)
 *
 * Mode selection via cmd_reg0 bits [31:30]:
 *   0b01  → PIO  mode  (pio_handle_trigger  → SC_THREAD)
 *   0b00  → CDMA mode  (cdma_handle_trigger → acmd_dma_ engine)
 *
 * Spec compliance fixes applied:
 *   PIO-3  : ctrl_config work mode validated before PIO trigger
 *   PIO-4  : ctrl_busy guard retained as implementation-defined safeguard
 *   PIO-5  : cmd_status_ptr (0x040) / cmd_status (0x044) indirect model
 *   PIO-6  : mb_xip_en forwarded through pio_dma_transfer
 *   PIO-A  : per-command register snapshot scope narrowed to only the
 *            registers required by that command type (Section 4.4.2.1)
 *   PIO-D  : reserved bits [29:27] and [23] masked before use (Table 4.10)
 *   CDMA-1 : acmd_command_flags_t UB removed – replaced with inline helpers
 *   CDMA-3 : ctrl_config work mode validated before CDMA trigger
 *   CDMA-4 : DESTABILIZE_RESET renamed to JEDEC_RESET (0x1101)
 *   CDMA-7 : all threads checked for completion, not just first active one
 *   CDMA-8 : BUS_ERROR (bit 1) used in execute_read/program, not DEVICE_ERROR
 *   CDMA-9 : CTRL_BUSY_BIT set/cleared correctly in both PIO and CDMA paths
 *   CDMA-C : MAX_CDMA_THREADS corrected to 8 (TRD_NUM is 3-bit, Table 4.3)
 *   CDMA-H : cdma_handle_trigger checks per-thread idle via TRD_STATUS;
 *            sets CMD_IGNORED in intr_status (0x110) if thread is busy
 *   CDMA-I : REG_INTR_STATUS (0x110) and CMD_IGNORED_BIT added to register map
 *   CDMA-W1: check_thread_idle uses 1-bit-per-thread stride
 *   XIP-A  : validate_descriptor rejects non-READ commands with MB_XIP_EN set
 *   R-2    : CTRL_BUSY_BIT position annotated as unverified (assumed bit 8)
 *   NEW-1  : CMD_IGNORED_BIT position annotated as unverified (assumed bit 0)
 *   NEW-7  : intr_status interrupt-pin routing annotated as unverified
 *   LOG-1  : [CDMA_DONE] diagnostic correctly reflects COMPLETE vs ERROR
 * =========================================================================
 ***************************************************************************/

    #ifndef CDNS_XSPI_ACMD_H
    #define CDNS_XSPI_ACMD_H

    // ── SystemC / TLM ────────────────────────────────────────────────────────────
    #include <systemc>
    #include <tlm>
    #include <tlm_utils/simple_target_socket.h>
    #include <tlm_utils/simple_initiator_socket.h>

    // ── STL ──────────────────────────────────────────────────────────────────────
    #include <cstdint>
    #include <map>
    #include <vector>
    #include <functional>
    #include <iostream>
    #include <iomanip>
    //#include <cstring>
    //#include <chrono>
    #include <thread>

    namespace mylibrary {

    // =============================================================================
    //  SECTION 1 – SHARED CONSTANTS
    // =============================================================================

    // ── Register offsets (Section 4.4) ───────────────────────────────────────────
    constexpr uint32_t REG_CMD_REG0        = 0x000;   // Trigger register (write last)
    constexpr uint32_t REG_CMD_REG1        = 0x004;   // xSPI Address Lower
    constexpr uint32_t REG_CMD_REG2        = 0x008;   // SYS_ADDR_PTR_L / Descriptor addr LSB
    constexpr uint32_t REG_CMD_REG3        = 0x00C;   // SYS_ADDR_PTR_H / Descriptor addr MSB
    constexpr uint32_t REG_CMD_REG4        = 0x010;   // DATA_CNT / SECT_CNT
    constexpr uint32_t REG_CMD_REG5        = 0x014;   // xSPI Address Upper (PIO only)

    // ── Status / interrupt registers ─────────────────────────────────────────────
    // Section 4.4.3: thread status obtained via two-register indirect access:
    //   write thread_id to REG_CMD_STATUS_PTR (0x040), then read REG_CMD_STATUS (0x044)
    constexpr uint32_t REG_CMD_STATUS_PTR  = 0x040;   // cmd_status_ptr  – thread selector
    constexpr uint32_t REG_CMD_STATUS      = 0x044;   // cmd_status      – selected thread status

    constexpr uint32_t REG_CTRL_STATUS     = 0x100;   // ctrl_status (ctrl_busy at bit 8)
    constexpr uint32_t REG_TRD_STATUS      = 0x104;   // trd_status (1 bit per thread, set=busy)
    constexpr uint32_t REG_INTR_STATUS     = 0x110;   // intr_status – CMD_IGNORED at bit 0
    constexpr uint32_t REG_TRD_COMP_INT    = 0x120;   // trd_comp_intr_status  (W1C)
    constexpr uint32_t REG_TRD_ERR_INT     = 0x130;   // trd_error_intr_status (W1C)
    constexpr uint32_t REG_CTRL_CONFIG     = 0x230;   // ctrl_config (work_mode at bits [6:5])

    // ── ctrl_config.work_mode (2 bits [6:5]) — register reference ──────────────
    constexpr uint32_t WORK_MODE_MASK      = (0x3u << 5);   // 0x60
    constexpr uint32_t WORK_MODE_ACMD      = (0x3u << 5);   // 2'b11: PIO + CDMA (cmd_reg0[31:30])

    // ── ctrl_status bits ─────────────────────────────────────────────────────────
    // R-2 NOTE: The bit position of ctrl_busy within ctrl_status could not be
    // confirmed from the provided spec screenshots.  Bit 8 is used here based on
    // the surrounding register map layout; verify against the full ctrl_status
    // register table if the spec document is available.
    constexpr uint32_t CTRL_BUSY_BIT       = (1u << 8);  // bit 8 = ctrl_busy (unverified – R-2)

    // ── cmd_reg0 mode field bits [31:30] ─────────────────────────────────────────
    constexpr uint32_t CMD_REG0_MODE_CDMA  = 0x0;    // 0b00 → CDMA mode
    constexpr uint32_t CMD_REG0_MODE_PIO   = 0x1;    // 0b01 → PIO  mode

    // ── Maximum thread counts ─────────────────────────────────────────────────────
    // CDMA-C FIX: TRD_NUM is a 3-bit field [26:24] (Table 4.3), so both PIO and
    // CDMA share the same physical limit of 8 addressable threads.
    constexpr uint8_t  MAX_THREADS         = 8;      // TRD_NUM field is 3 bits [26:24]
    constexpr uint8_t  MAX_CDMA_THREADS    = 8;      // CDMA-C: corrected from 16 → 8

    // ── intr_status register (CDMA-I FIX) ────────────────────────────────────────
    // Section 4.4.1: A write to cmd_reg0 while the target thread is already busy
    // sets the CMD_IGNORED flag in intr_status and does NOT start a new operation.
    // NEW-1 NOTE: The bit position of CMD_IGNORED within intr_status could not be
    // confirmed from the provided spec screenshots.  Bit 0 is assumed based on
    // common register layout conventions; verify against the full intr_status
    // register table if the spec document is available.  Same category as R-2.
    constexpr uint32_t CMD_IGNORED_BIT     = (1u << 0); // bit 0 = CMD_IGNORED (unverified – NEW-1)


    // =============================================================================
    //  SECTION 2 – COMMAND FLAGS INLINE ACCESSORS  (DEFECT CDMA-1 FIX)
    //
    //  The spec defines a 16-bit Command Flags field (Table 4.8).  Using a C++
    //  bitfield struct that spans uint8_t boundaries and accessing it via
    //  reinterpret_cast is undefined behaviour.  These inline helpers extract each
    //  field portably from the raw uint16_t value stored in the descriptor.
    // =============================================================================

    /// Extract Bank/CS field [2:0]
    inline uint8_t  flags_bank          (uint16_t f) { return  static_cast<uint8_t>(f & 0x07u); }
    /// [3] Reserved – not accessed in logic
    /// Extract SYS_PTR_CONT flag [4]
    inline bool     flags_sys_ptr_cont  (uint16_t f) { return ((f >>  4u) & 1u) != 0u; }
    /// Extract XSPI_PTR_CONT flag [5]
    inline bool     flags_xspi_ptr_cont (uint16_t f) { return ((f >>  5u) & 1u) != 0u; }
    /// Extract MB_XIP_EN flag [6]
    inline bool     flags_mb_xip_en     (uint16_t f) { return ((f >>  6u) & 1u) != 0u; }
    /// Extract MB_XIP_DIS flag [7]
    inline bool     flags_mb_xip_dis    (uint16_t f) { return ((f >>  7u) & 1u) != 0u; }
    /// Extract INT flag [8]
    inline bool     flags_int_flag      (uint16_t f) { return ((f >>  8u) & 1u) != 0u; }
    /// Extract CONT flag [9]
    inline bool     flags_cont          (uint16_t f) { return ((f >>  9u) & 1u) != 0u; }
    /// Extract DMA_SEL flag [10]  (0=Slave, 1=Master)
    inline bool     flags_dma_sel       (uint16_t f) { return ((f >> 10u) & 1u) != 0u; }

    /// Build a flags word from individual fields
    inline uint16_t flags_build(uint8_t bank, bool sys_ptr_cont, bool xspi_ptr_cont,
                                bool mb_xip_en, bool mb_xip_dis, bool int_flag,
                                bool cont, bool dma_sel)
    {
        uint16_t f = 0;
        f |= static_cast<uint16_t>(bank & 0x07u);
        if (sys_ptr_cont)  f |= (1u << 4u);
        if (xspi_ptr_cont) f |= (1u << 5u);
        if (mb_xip_en)     f |= (1u << 6u);
        if (mb_xip_dis)    f |= (1u << 7u);
        if (int_flag)      f |= (1u << 8u);
        if (cont)          f |= (1u << 9u);
        if (dma_sel)       f |= (1u << 10u);
        return f;
    }


    // =============================================================================
    //  SECTION 3 – PIO TYPES  (Section 4.4.2)
    // =============================================================================

    // PIO Command Types  (Table 4.10, CMD_TYPE field [15:0])
    enum pio_cmd_type_e : uint16_t {
        PIO_CMD_RESET_SOFT   = 0x1100,
        PIO_CMD_RESET_JEDEC  = 0x1101,
        PIO_CMD_SECTOR_ERASE = 0x1000,
        PIO_CMD_CHIP_ERASE   = 0x1001,
        PIO_CMD_PROGRAM      = 0x2100,
        PIO_CMD_READ         = 0x2200
    };

    // PIO thread state machine
    enum class pio_thread_state_e {
        IDLE = 0,
        BUSY,
        COMPLETE,
        FAIL
    };

    // PIO per-thread status register (Table 4.17)
    // Bit layout matches cmd_status register at REG_CMD_STATUS (0x044)
    struct pio_status_reg_t {
        bool    cmd_error;       // [0]  CMD_ERROR
        bool    bus_error;       // [1]  BUS_ERROR
        bool    crc_error;       // [2]  CRC_ERROR
        bool    dqs_error;       // [3]  DQS_ERROR
        bool    device_error;    // [4]  DEVICE_ERROR
        bool    ecc_corr_error;  // [5]  ECC_CORR_ERROR
        bool    fail;            // [14] FAIL
        bool    complete;        // [15] COMPLETE
        uint8_t ecc_stat;        // [23:16] ECC_STAT

        pio_status_reg_t()
            : cmd_error(false), bus_error(false), crc_error(false),
            dqs_error(false), device_error(false),
            ecc_corr_error(false), fail(false), complete(false),
            ecc_stat(0) {}

        uint32_t to_reg() const {
            uint32_t val = 0;
            if (cmd_error)      val |= (1u << 0);
            if (bus_error)      val |= (1u << 1);
            if (crc_error)      val |= (1u << 2);
            if (dqs_error)      val |= (1u << 3);
            if (device_error)   val |= (1u << 4);
            if (ecc_corr_error) val |= (1u << 5);
            // bits [13:6] reserved
            if (fail)           val |= (1u << 14);
            if (complete)       val |= (1u << 15);
            val |= (static_cast<uint32_t>(ecc_stat) << 16);
            return val;
        }
    };

    // PIO per-thread execution context
    struct pio_thread_ctx_t {
        uint8_t            thread_id;
        pio_thread_state_e state;
        // Snapshot of cmd_reg1..5 taken when cmd_reg0 is written (spec: write last)
        uint32_t           cmd_reg1;   // xSPI address lower
        uint32_t           cmd_reg2;   // host memory address lower
        uint32_t           cmd_reg3;   // host memory address upper
        uint32_t           cmd_reg4;   // data / sector count (value – 1)
        uint32_t           cmd_reg5;   // xSPI address upper
        // Fields decoded from cmd_reg0
        uint8_t            bank_cs;
        bool               dma_sel;
        bool               int_flag;
        bool               mb_xip_dis;
        bool               mb_xip_en;
        uint16_t           cmd_type;
        pio_status_reg_t   status;

        pio_thread_ctx_t()
            : thread_id(0), state(pio_thread_state_e::IDLE),
            cmd_reg1(0), cmd_reg2(0), cmd_reg3(0),
            cmd_reg4(0), cmd_reg5(0),
            bank_cs(0), dma_sel(true), int_flag(false),
            mb_xip_dis(false), mb_xip_en(false), cmd_type(0) {}
    };


    // =============================================================================
    //  SECTION 4 – CDMA / ACMD TYPES  (Section 4.4.1)
    // =============================================================================

    // CDMA Command Types  (Table 4.7)
    enum class acmd_command_type_e : uint16_t {
        ERASE_SECTORS   = 0x1000,   // Erase number of sequential sectors
        FULL_CHIP_ERASE = 0x1001,   // Full chip erase
        DEVICE_RESET    = 0x1100,   // Device Reset
        JEDEC_RESET     = 0x1101,   // Device JEDEC Reset (CDMA-4 fix)
        PROGRAM         = 0x2100,   // Program (write) data
        READ            = 0x2200    // Read data
    };

    // CDMA Descriptor Status field (Table 4.9)
    // Stored as a plain uint32_t in the descriptor; bit positions are spec-defined.
    namespace cdma_status_bits {
        constexpr uint32_t DSC_ERROR      = (1u << 0);   // Invalid descriptor sequence
        constexpr uint32_t BUS_ERROR      = (1u << 1);   // System bus error
        constexpr uint32_t CRC_ERROR      = (1u << 2);   // CRC error
        constexpr uint32_t DQS_ERROR      = (1u << 3);   // DQS error
        constexpr uint32_t DEVICE_ERROR   = (1u << 4);   // Device error
        constexpr uint32_t ECC_CORR_ERROR = (1u << 5);   // ECC correctable error
        // bits [13:6] reserved
        constexpr uint32_t FAIL           = (1u << 14);  // Operation failed
        constexpr uint32_t COMPLETE       = (1u << 15);  // Operation complete (set even on failure)
        // bits [23:16] = ECC_STAT
        // bits [31:24] reserved
    }

    // CDMA Command Descriptor (Table 4.6)
    // 64-byte structure, must be 64-byte aligned in memory.
    struct acmd_command_descriptor_t {
        uint64_t next_pointer;             // Word 0: Next descriptor address (0 = end of chain)
        uint64_t system_memory_pointer;    // Word 1: Host/system memory address
        uint64_t xspi_pointer;             // Word 2: xSPI device address
        uint64_t reserved_3;               // Word 3: Reserved
        // Word 4:
        uint16_t command_type;             // [15:0]  Command Type
        uint16_t command_flags;            // [31:16] Command Flags (use flags_* helpers)
        uint16_t command_counter;          // [47:32] Byte count-1 (or sector count for ERASE)
        uint16_t reserved_4_upper;         // [63:48] Reserved
        // Word 5:
        uint32_t status;                   // [31:0]  Status (written back by controller)
        uint32_t reserved_5_upper;         // [63:32] Reserved

        acmd_command_descriptor_t()
            : next_pointer(0), system_memory_pointer(0),
            xspi_pointer(0), reserved_3(0),
            command_type(0), command_flags(0),
            command_counter(0), reserved_4_upper(0),
            status(0), reserved_5_upper(0) {}
    } __attribute__((aligned(64)));

    // CDMA thread state machine
    enum class acmd_thread_state_e {
        IDLE = 0,
        FETCHING_DESCRIPTOR,
        EXECUTING_COMMAND,
        UPDATING_STATUS,
        ERROR,
        COMPLETE
    };

    // CDMA per-thread execution context
    class acmd_dma_thread_t {
    public:
        uint8_t                   thread_number;
        acmd_thread_state_e       state;
        uint64_t                  descriptor_head_address;
        acmd_command_descriptor_t current_desc;
        bool                      active;
        bool                      had_error;   // LOG-1: tracks whether thread ended in error

        acmd_dma_thread_t()
            : thread_number(0), state(acmd_thread_state_e::IDLE),
            descriptor_head_address(0), active(false), had_error(false) {}
    };


    // =============================================================================
    //  SECTION 5 – cdns_xspi_acmd_dma  ( CDMA engine class)
    // =============================================================================

    class cdns_xspi_acmd_dma {
    public:
        cdns_xspi_acmd_dma();
        ~cdns_xspi_acmd_dma();

        // ── Mode helpers ──────────────────────────────────────────────────────────
        void enable_acmd_mode();
        void disable_acmd_mode();
        bool is_acmd_mode_enabled() const;

        // ── Thread management ─────────────────────────────────────────────────────
        bool                start_thread(uint8_t thread_num, uint64_t descriptor_head_addr);
        acmd_thread_state_e get_thread_state(uint8_t thread_num) const;
        bool                is_thread_busy(uint8_t thread_num) const;

        // ── Descriptor operations ─────────────────────────────────────────────────
        bool fetch_descriptor(uint64_t address, acmd_command_descriptor_t& descriptor);
        bool validate_descriptor(const acmd_command_descriptor_t& descriptor);
        bool update_descriptor_status(uint64_t address, uint32_t status);

        // ── Command execution ─────────────────────────────────────────────────────
        bool execute_descriptor(acmd_dma_thread_t& thread);
        bool execute_read_command(acmd_command_descriptor_t& descriptor);
        bool execute_program_command(acmd_command_descriptor_t& descriptor);
        bool execute_erase_command(acmd_command_descriptor_t& descriptor);
        bool execute_reset_command(acmd_command_descriptor_t& descriptor);

        // ── DMA transfer operations ───────────────────────────────────────────────
        bool dma_read_from_xspi(uint64_t xspi_addr, uint64_t sys_addr,
                                uint32_t byte_count, bool use_dma_master);
        bool dma_write_to_xspi(uint64_t sys_addr, uint64_t xspi_addr,
                                uint32_t byte_count, bool use_dma_master);

        // ── Callback registration ─────────────────────────────────────────────────
        void set_register_callbacks(
            std::function<void(uint32_t, uint32_t)> write_reg,
            std::function<uint32_t(uint32_t)>       read_reg);
        void set_dma_callbacks(
            std::function<bool(uint64_t, uint64_t, uint32_t)> dma_read,
            std::function<bool(uint64_t, uint64_t, uint32_t)> dma_write);

        // ── Interrupt / status management ─────────────────────────────────────────
        uint16_t check_completion_interrupts();
        uint16_t check_error_interrupts();
        void     clear_completion_interrupt(uint8_t thread_num);
        void     clear_error_interrupt(uint8_t thread_num);

        // ── Main processing loop ──────────────────────────────────────────────────
        void process_threads();
        bool wait_for_thread_completion(uint8_t thread_num, uint32_t timeout_ms = 0);

        // ── Public data (exposed for TLM module synchronisation) ─────────────────
        std::vector<acmd_dma_thread_t> threads_;
        uint16_t completion_interrupts_;   // mirrors REG_TRD_COMP_INT bits
        uint16_t error_interrupts_;        // mirrors REG_TRD_ERR_INT  bits

    private:
        // ── Private register helpers ──────────────────────────────────────────────
        void     write_cmd_reg0(uint32_t v);
        void     write_cmd_reg2(uint32_t v);
        void     write_cmd_reg3(uint32_t v);
        uint32_t read_ctrl_status();
        uint32_t read_thread_status();
        bool     check_controller_busy();
        bool     check_thread_idle(uint8_t thread_num);

        uint8_t descriptor_buffer_[64] __attribute__((aligned(64)));

        std::function<void(uint32_t, uint32_t)>           write_register_;
        std::function<uint32_t(uint32_t)>                 read_register_;
        std::function<bool(uint64_t, uint64_t, uint32_t)> dma_read_callback_;
        std::function<bool(uint64_t, uint64_t, uint32_t)> dma_write_callback_;
    };


    // =============================================================================
    //  SECTION 6 – cdns_xspi_acmd_tlm  (SystemC module, unified PIO + CDMA)
    //
    //  PIO  path : cmd_reg0[31:30]==0b01 → pio_handle_trigger()  → SC_THREAD
    //  CDMA path : cmd_reg0[31:30]==0b00 → cdma_handle_trigger() → acmd_dma_
    // =============================================================================

    SC_MODULE(cdns_xspi_acmd_tlm) {

        // ── TLM sockets ───────────────────────────────────────────────────────────
        tlm_utils::simple_target_socket<cdns_xspi_acmd_tlm>    apb_target_socket;
        tlm_utils::simple_initiator_socket<cdns_xspi_acmd_tlm> axi_master_socket;

        // ── Hardware signals ──────────────────────────────────────────────────────
        sc_core::sc_out<bool> interrupt_out;

        // ── Embedded non-SC CDMA engine ───────────────────────────────────────────
        cdns_xspi_acmd_dma acmd_dma_;

        SC_CTOR(cdns_xspi_acmd_tlm);
        ~cdns_xspi_acmd_tlm();

        // ── CDMA DMA callbacks ────────────────────────────────────────────────────
        void set_dma_callbacks(
            std::function<bool(uint64_t, uint64_t, uint32_t)> dma_read,
            std::function<bool(uint64_t, uint64_t, uint32_t)> dma_write);

    private:
        // ── Shared register file ──────────────────────────────────────────────────
        std::map<uint32_t, uint32_t> registers_;
        sc_core::sc_mutex             reg_mutex_;

        // ── Per-thread status storage (indexed by thread_id) ─────────────────────
        // Accessed indirectly via REG_CMD_STATUS_PTR / REG_CMD_STATUS (Section 4.4.3)
        pio_status_reg_t thread_status_[MAX_CDMA_THREADS];

        // ── PIO thread array ──────────────────────────────────────────────────────
        std::vector<pio_thread_ctx_t> pio_threads_;

        // ── Initialisation ────────────────────────────────────────────────────────
        void init_registers();
        void pio_init_threads();

        // ── Register callbacks exposed to CDMA engine ─────────────────────────────
        void     reg_cb_write(uint32_t off, uint32_t val);
        uint32_t reg_cb_read(uint32_t off);

        // ── APB TLM transport ─────────────────────────────────────────────────────
        void apb_b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);

        // ── Register access ───────────────────────────────────────────────────────
        void     register_write(uint32_t addr, uint32_t data);
        uint32_t register_read(uint32_t addr);

        // ── Trigger dispatch ──────────────────────────────────────────────────────
        void handle_trigger(uint32_t cmd_reg0);
        void pio_handle_trigger(uint32_t cmd_reg0);
        void cdma_handle_trigger(uint32_t cmd_reg0);

        // ── SystemC execution thread (polls every 10 ns) ──────────────────────────
        void command_execution_thread();

        // ── PIO command dispatch and implementations ──────────────────────────────
        void pio_execute_command(pio_thread_ctx_t& thread);
        bool pio_cmd_read(pio_thread_ctx_t& t);
        bool pio_cmd_program(pio_thread_ctx_t& t);
        bool pio_cmd_sector_erase(pio_thread_ctx_t& t);
        bool pio_cmd_chip_erase(pio_thread_ctx_t& t);
        bool pio_cmd_reset(pio_thread_ctx_t& t);
        bool pio_dma_transfer(uint64_t src, uint64_t dst, uint32_t size, bool mb_xip_en);

        // ── CTRL_BUSY helpers ─────────────────────────────────────────────────────
        void set_ctrl_busy(bool busy);
        bool any_thread_active() const;

        // ── CDMA-I: intr_status CMD_IGNORED signaling ─────────────────────────────
        void signal_cmd_ignored();

        // ── Interrupt driver ──────────────────────────────────────────────────────
        void update_interrupt();
    };

    } // namespace mylibrary
    #endif // CDNS_XSPI_ACMD_H