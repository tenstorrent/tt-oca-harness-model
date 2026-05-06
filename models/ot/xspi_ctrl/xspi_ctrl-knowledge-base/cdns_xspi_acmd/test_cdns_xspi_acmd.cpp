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
 * Unified Testbench – cdns_xspi_acmd_tlm
 *
 * PART A – PIO mode (Section 4.4.2)
 *   PA-1  READ           CMD=0x2200
 *   PA-2  PROGRAM        CMD=0x2100
 *   PA-3  SECTOR ERASE   CMD=0x1000
 *   PA-4  CHIP ERASE     CMD=0x1001
 *   PA-5  SOFT RESET     CMD=0x1100
 *   PA-6  JEDEC RESET    CMD=0x1101
 *   PA-7  Interrupt handling (assert, W1C clear, de-assert)
 *
 * PART B – CDMA mode (Section 4.4.1)
 *   PB-1  READ via descriptor
 *   PB-2  PROGRAM via descriptor
 *   PB-3  ERASE SECTORS via descriptor
 *   PB-4  FULL CHIP ERASE via descriptor
 *   PB-5  DEVICE RESET via descriptor
 *   PB-6  Descriptor chain (CONT flag, 2 chained READs)
 *   PB-7  Interrupt from descriptor (int_flag=1)
 *   PB-8  CMD_IGNORED: write to busy thread sets intr_status[0]  (CDMA-H/I)
 *   PB-9  XIP-A: MB_XIP_EN on non-READ descriptor rejected
 *
 * Status reading (Section 4.4.3):
 *   Thread status is read via two-register indirect access:
 *     write thread_id → REG_CMD_STATUS_PTR (0x040)
 *     read  status    ← REG_CMD_STATUS     (0x044)
 * =========================================================================
 ***************************************************************************/

#include "cdns_xspi_acmd.h"

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <cstring>
#include <string>

using namespace sc_core;
using namespace mylibrary;

// ============================================================================
//  Helper functions
// ============================================================================

static void banner(const std::string& title, char fill = '=', int width = 62) {
    std::string line(width, fill);
    std::cout << "\n" << line << "\n  " << title << "\n" << line << std::endl;
}
static void print_pass(const std::string& msg) {
    std::cout << "  \u2713 " << msg << std::endl;
}
static void print_fail(const std::string& msg) {
    std::cerr << "  \u2717 " << msg << std::endl;
}
static std::string hex32(uint32_t v) {
    std::ostringstream ss;
    ss << "0x" << std::hex << std::setfill('0') << std::setw(8) << v;
    return ss.str();
}
static void dump_mem(const uint8_t* data, uint32_t size, const std::string& label) {
    std::cout << "\n  " << label << " (" << size << " bytes):\n";
    for (uint32_t i = 0; i < size; ++i) {
        if (i % 16 == 0)
            std::cout << "    " << std::hex << std::setfill('0')
                      << std::setw(4) << i << ": ";
        std::cout << std::setw(2) << static_cast<int>(data[i]) << " ";
        if ((i + 1) % 16 == 0 || i == size - 1) std::cout << "\n";
    }
    std::cout << std::dec;
}


// ============================================================================
//  SECTION 1 – TLM memory model (target for PIO AXI master DMA path)
// ============================================================================

SC_MODULE(memory_model) {
    tlm_utils::simple_target_socket<memory_model> target_socket;

    SC_CTOR(memory_model)
        : target_socket("target_socket"), mem_size_(16u * 1024u * 1024u)
    {
        target_socket.register_b_transport(this, &memory_model::b_transport);
        mem_ = new uint8_t[mem_size_];
        memset(mem_, 0xFF, mem_size_);
        std::cout << "[MEM] TLM memory: 16 MB\n";
    }
    ~memory_model() { delete[] mem_; }

    void b_transport(tlm::tlm_generic_payload& trans, sc_time& delay) {
        sc_dt::uint64  addr = trans.get_address();
        unsigned char* ptr  = trans.get_data_ptr();
        unsigned int   len  = trans.get_data_length();
        if (addr + len > mem_size_) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE); return;
        }
        if (trans.get_command() == tlm::TLM_READ_COMMAND)
            memcpy(ptr, &mem_[addr], len);
        else
            memcpy(&mem_[addr], ptr, len);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        delay += sc_time(static_cast<double>(len), SC_NS);
    }

    void fill(uint64_t addr, uint32_t size, uint8_t base) {
        for (uint32_t i = 0; i < size && (addr + i) < mem_size_; ++i)
            mem_[addr + i] = static_cast<uint8_t>(base + (i & 0xFFu));
    }

private:
    uint8_t*  mem_;
    uint64_t  mem_size_;
};


// ============================================================================
//  SECTION 2 – MockHardware (CDMA DMA callbacks)
// ============================================================================

class MockHardware {
public:
    static constexpr uint64_t SYS_SIZE  = 16u * 1024u * 1024u;
    static constexpr uint64_t XSPI_SIZE = 16u * 1024u * 1024u;

    MockHardware() {
        sys_mem_.resize(SYS_SIZE,   0x00u);
        xspi_mem_.resize(XSPI_SIZE, 0xFFu);
        std::cout << "[MOCK_HW] sys=" << SYS_SIZE/1024
                  << " KB  xSPI=" << XSPI_SIZE/1024 << " KB\n";
    }

    // dma_read(src, dst, size):
    //   (a) dst > SYS_SIZE → dst is a native host pointer (descriptor fetch)
    //   (b) src < XSPI_SIZE, dst < SYS_SIZE → xSPI → system  (READ)
    //   (c) both in SYS → system → system (status writeback)
    bool dma_read(uint64_t src, uint64_t dst, uint32_t size) {
        if (dst > SYS_SIZE) {
            if (src + size > SYS_SIZE) { std::cerr << "[HW] dma_read ptr OOB\n"; return false; }
            memcpy(reinterpret_cast<void*>(dst), &sys_mem_[src], size);
            return true;
        }
        if (src < XSPI_SIZE && dst < SYS_SIZE) {
            if (src + size > XSPI_SIZE || dst + size > SYS_SIZE) {
                std::cerr << "[HW] dma_read xspi→sys OOB\n"; return false;
            }
            memcpy(&sys_mem_[dst], &xspi_mem_[src], size);
            return true;
        }
        if (src < SYS_SIZE && dst < SYS_SIZE) {
            if (src + size > SYS_SIZE || dst + size > SYS_SIZE) {
                std::cerr << "[HW] dma_read sys→sys OOB\n"; return false;
            }
            memmove(&sys_mem_[dst], &sys_mem_[src], size);
            return true;
        }
        std::cerr << "[HW] dma_read unhandled src=0x" << std::hex << src
                  << " dst=0x" << dst << std::dec << "\n";
        return false;
    }

    // dma_write(src, dst, size):
    //   (a) src > SYS_SIZE → src is a native host pointer (status writeback buffer)
    //   (b) src < SYS_SIZE, dst < XSPI_SIZE → system → xSPI  (PROGRAM)
    //   (c) both in SYS → system → system
    bool dma_write(uint64_t src, uint64_t dst, uint32_t size) {
        if (src > SYS_SIZE) {
            if (dst + size > SYS_SIZE) { std::cerr << "[HW] dma_write ptr OOB\n"; return false; }
            memcpy(&sys_mem_[dst], reinterpret_cast<void*>(src), size);
            return true;
        }
        if (src < SYS_SIZE && dst < XSPI_SIZE) {
            if (src + size > SYS_SIZE || dst + size > XSPI_SIZE) {
                std::cerr << "[HW] dma_write sys→xspi OOB\n"; return false;
            }
            memcpy(&xspi_mem_[dst], &sys_mem_[src], size);
            return true;
        }
        if (src < SYS_SIZE && dst < SYS_SIZE) {
            if (src + size > SYS_SIZE || dst + size > SYS_SIZE) {
                std::cerr << "[HW] dma_write sys→sys OOB\n"; return false;
            }
            memmove(&sys_mem_[dst], &sys_mem_[src], size);
            return true;
        }
        std::cerr << "[HW] dma_write unhandled src=0x" << std::hex << src
                  << " dst=0x" << dst << std::dec << "\n";
        return false;
    }

    uint8_t* sys_raw()  { return sys_mem_.data();  }
    uint8_t* xspi_raw() { return xspi_mem_.data(); }

private:
    std::vector<uint8_t> sys_mem_;
    std::vector<uint8_t> xspi_mem_;
};


// ============================================================================
//  SECTION 3 – Descriptor builder
//  Uses flags_build() helper so no bitfield UB
// ============================================================================

static void build_descriptor(uint8_t*            sys_mem,
                              uint64_t            desc_addr,
                              uint64_t            next_ptr,
                              uint64_t            sys_mem_ptr,
                              uint64_t            xspi_ptr,
                              acmd_command_type_e cmd_type,
                              uint16_t            cmd_counter,
                              uint8_t             bank,
                              bool                dma_sel,
                              bool                cont,
                              bool                int_flag)
{
    auto* d = reinterpret_cast<acmd_command_descriptor_t*>(sys_mem + desc_addr);
    d->next_pointer          = next_ptr;
    d->system_memory_pointer = sys_mem_ptr;
    d->xspi_pointer          = xspi_ptr;
    d->command_type          = static_cast<uint16_t>(cmd_type);
    d->command_counter       = cmd_counter;
    d->reserved_3            = 0;
    d->reserved_4_upper      = 0;
    d->status                = 0;

    // Use flags_build() to avoid bitfield struct UB (DEFECT CDMA-1 fix)
    d->command_flags = flags_build(bank,
                                   /*sys_ptr_cont=*/false,
                                   /*xspi_ptr_cont=*/false,
                                   /*mb_xip_en=*/false,
                                   /*mb_xip_dis=*/false,
                                   int_flag, cont, dma_sel);
}


// ============================================================================
//  SECTION 4 – test_driver SC_MODULE
// ============================================================================

SC_MODULE(test_driver) {

    tlm_utils::simple_initiator_socket<test_driver> apb_init_socket;
    sc_in<bool>   interrupt_in;
    MockHardware* hw_;   // set by sc_main before sc_start()

    SC_CTOR(test_driver)
        : apb_init_socket("apb_init_socket"),
          interrupt_in("interrupt_in"),
          hw_(nullptr)
    {
        SC_THREAD(run_pio_tests);
        SC_THREAD(run_cdma_tests);
    }

    // =========================================================================
    //  PART A – PIO tests
    // =========================================================================

    void run_pio_tests() {
        wait(sc_time(200, SC_NS));
        banner("PART A – PIO MODE TESTS");

        // PIO requires global work_mode 2'b11 at ctrl_config[6:5] (same as CDMA)
        write_reg(REG_CTRL_CONFIG, WORK_MODE_ACMD);
        wait(sc_time(10, SC_NS));

        pa1_read();
        pa2_program();
        pa3_sector_erase();
        pa4_chip_erase();
        pa5_soft_reset();
        pa6_jedec_reset();
        pa7_interrupt();

        banner("PART A – PIO TESTS COMPLETE");
        pio_done_.notify();
    }

    // ── PA-1: READ ────────────────────────────────────────────────────────────
    void pa1_read() {
        banner("PA-1: READ (CMD=0x2200)", '-', 50);
        // Write cmd_reg1..5 first; cmd_reg0 written last (spec requirement)
        write_reg(REG_CMD_REG1, 0x00001000u);   // xSPI src [31:0]
        write_reg(REG_CMD_REG5, 0x00000000u);   // xSPI src [63:32]
        write_reg(REG_CMD_REG2, 0x00020000u);   // host dst [31:0]
        write_reg(REG_CMD_REG3, 0x00000000u);   // host dst [63:32]
        write_reg(REG_CMD_REG4, 255u);          // 256 bytes – 1

        uint32_t cmd0 = (CMD_REG0_MODE_PIO << 30) |
                        (0u << 24) |    // TRD_NUM = 0
                        (0u << 20) |    // BANK/CS = 0
                        (1u << 19) |    // DMA_SEL = Master
                        (1u << 18) |    // INT     = 1
                        0x2200u;        // CMD_TYPE = READ
        write_reg(REG_CMD_REG0, cmd0);
        wait_for_thread(0);

        uint32_t st = read_thread_status(0);
        (st & (1u << 15)) ? print_pass("READ COMPLETE status=" + hex32(st))
                           : print_fail("READ FAIL    status=" + hex32(st));
    }

    // ── PA-2: PROGRAM ─────────────────────────────────────────────────────────
    void pa2_program() {
        banner("PA-2: PROGRAM (CMD=0x2100)", '-', 50);
        write_reg(REG_CMD_REG1, 0x00002000u);
        write_reg(REG_CMD_REG5, 0x00000000u);
        write_reg(REG_CMD_REG2, 0x00030000u);
        write_reg(REG_CMD_REG3, 0x00000000u);
        write_reg(REG_CMD_REG4, 127u);          // 128 bytes – 1

        uint32_t cmd0 = (CMD_REG0_MODE_PIO << 30) |
                        (1u << 24) |    // TRD_NUM = 1
                        (0u << 20) |
                        (1u << 19) |
                        (1u << 18) |
                        0x2100u;
        write_reg(REG_CMD_REG0, cmd0);
        wait_for_thread(1);

        uint32_t st = read_thread_status(1);
        (st & (1u << 15)) ? print_pass("PROGRAM COMPLETE status=" + hex32(st))
                           : print_fail("PROGRAM FAIL    status=" + hex32(st));
    }

    // ── PA-3: SECTOR ERASE ────────────────────────────────────────────────────
    void pa3_sector_erase() {
        banner("PA-3: SECTOR ERASE (CMD=0x1000)", '-', 50);
        write_reg(REG_CMD_REG1, 0x00010000u);   // xSPI address lower
        write_reg(REG_CMD_REG5, 0x00000000u);   // xSPI address upper
        write_reg(REG_CMD_REG4, 3u);            // 4 sectors (4–1=3)

        // SECTOR ERASE: cmd_reg0, cmd_reg1, cmd_reg4, cmd_reg5 required (Section 4.4.2.1)
        uint32_t cmd0 = (CMD_REG0_MODE_PIO << 30) |
                        (2u << 24) |    // TRD_NUM = 2
                        (1u << 20) |    // BANK/CS = 1
                        (1u << 18) |    // INT     = 1
                        0x1000u;
        write_reg(REG_CMD_REG0, cmd0);
        wait_for_thread(2);

        uint32_t st = read_thread_status(2);
        (st & (1u << 15)) ? print_pass("SECTOR ERASE COMPLETE status=" + hex32(st))
                           : print_fail("SECTOR ERASE FAIL    status=" + hex32(st));
    }

    // ── PA-4: CHIP ERASE ──────────────────────────────────────────────────────
    void pa4_chip_erase() {
        banner("PA-4: CHIP ERASE (CMD=0x1001)", '-', 50);
        // CHIP ERASE: only cmd_reg0 required (Section 4.4.2.1)
        uint32_t cmd0 = (CMD_REG0_MODE_PIO << 30) |
                        (3u << 24) |    // TRD_NUM = 3
                        (1u << 18) |    // INT     = 1
                        0x1001u;
        write_reg(REG_CMD_REG0, cmd0);
        wait_for_thread(3);

        uint32_t st = read_thread_status(3);
        (st & (1u << 15)) ? print_pass("CHIP ERASE COMPLETE status=" + hex32(st))
                           : print_fail("CHIP ERASE FAIL    status=" + hex32(st));
    }

    // ── PA-5: SOFT RESET ──────────────────────────────────────────────────────
    void pa5_soft_reset() {
        banner("PA-5: SOFT RESET (CMD=0x1100)", '-', 50);
        // RESET: only cmd_reg0 required (Section 4.4.2.1)
        uint32_t cmd0 = (CMD_REG0_MODE_PIO << 30) |
                        (4u << 24) |    // TRD_NUM = 4
                        0x1100u;
        write_reg(REG_CMD_REG0, cmd0);
        wait_for_thread(4);

        uint32_t st = read_thread_status(4);
        (st & (1u << 15)) ? print_pass("SOFT RESET COMPLETE status=" + hex32(st))
                           : print_fail("SOFT RESET FAIL    status=" + hex32(st));
    }

    // ── PA-6: JEDEC RESET ─────────────────────────────────────────────────────
    void pa6_jedec_reset() {
        banner("PA-6: JEDEC RESET (CMD=0x1101)", '-', 50);
        uint32_t cmd0 = (CMD_REG0_MODE_PIO << 30) |
                        (5u << 24) |    // TRD_NUM = 5
                        0x1101u;
        write_reg(REG_CMD_REG0, cmd0);
        wait_for_thread(5);

        uint32_t st = read_thread_status(5);
        (st & (1u << 15)) ? print_pass("JEDEC RESET COMPLETE status=" + hex32(st))
                           : print_fail("JEDEC RESET FAIL    status=" + hex32(st));
    }

    // ── PA-7: Interrupt handling ──────────────────────────────────────────────
    void pa7_interrupt() {
        banner("PA-7: Interrupt handling", '-', 50);
        write_reg(REG_CMD_REG1, 0x00003000u);
        write_reg(REG_CMD_REG5, 0x00000000u);
        write_reg(REG_CMD_REG2, 0x00040000u);
        write_reg(REG_CMD_REG3, 0x00000000u);
        write_reg(REG_CMD_REG4, 63u);           // 64 bytes – 1

        uint32_t cmd0 = (CMD_REG0_MODE_PIO << 30) |
                        (6u << 24) |
                        (1u << 19) |
                        (1u << 18) |
                        0x2200u;
        write_reg(REG_CMD_REG0, cmd0);
        wait_for_thread(6);

        // Check interrupt line
        interrupt_in.read()
            ? print_pass("Interrupt asserted after PIO completion")
            : print_fail("Interrupt NOT asserted");

        // Check TRD_COMP_INT[6]
        uint32_t comp = read_reg(REG_TRD_COMP_INT);
        (comp & (1u << 6))
            ? print_pass("TRD_COMP_INT[6] set, value=" + hex32(comp))
            : print_fail("TRD_COMP_INT[6] NOT set, value=" + hex32(comp));

        // W1C clear
        write_reg(REG_TRD_COMP_INT, (1u << 6));
        wait(sc_time(20, SC_NS));

        uint32_t comp2 = read_reg(REG_TRD_COMP_INT);
        ((comp2 & (1u << 6)) == 0)
            ? print_pass("TRD_COMP_INT[6] cleared (W1C), value=" + hex32(comp2))
            : print_fail("TRD_COMP_INT[6] NOT cleared, value="   + hex32(comp2));
    }

    // =========================================================================
    //  PART B – CDMA tests
    // =========================================================================

    void run_cdma_tests() {
        wait(pio_done_);
        banner("PART B – CDMA MODE TESTS");

        // CDMA: global work_mode 2'b11 at ctrl_config[6:5]
        write_reg(REG_CTRL_CONFIG, WORK_MODE_ACMD);
        wait(sc_time(10, SC_NS));

        pb1_cdma_read();
        pb2_cdma_program();
        pb3_cdma_erase_sectors();
        pb4_cdma_full_chip_erase();
        pb5_cdma_device_reset();
        pb6_cdma_descriptor_chain();
        pb7_cdma_interrupt();
        pb8_cmd_ignored();   // CDMA-H/I: busy-thread write → CMD_IGNORED in intr_status
        pb9_xip_rejected();  // XIP-A: MB_XIP_EN on non-READ → DSC_ERROR

        banner("PART B – CDMA TESTS COMPLETE");
        banner("ALL TESTS COMPLETE", '*', 62);
        std::cout << "  Sim end time: " << sc_time_stamp() << "\n";
        sc_stop();
    }

    // ── PB-1: CDMA READ ───────────────────────────────────────────────────────
    void pb1_cdma_read() {
        banner("PB-1: CDMA READ (descriptor)", '-', 50);
        for (int i = 0; i < 256; ++i)
            hw_->xspi_raw()[0x1000 + i] = static_cast<uint8_t>(i);

        build_descriptor(hw_->sys_raw(),
            /*desc_addr=*/0x10000, /*next=*/0,
            /*sys_ptr=*/0x20000, /*xspi_ptr=*/0x1000,
            acmd_command_type_e::READ, /*counter=*/255,
            /*bank=*/0, /*dma_sel=*/true, /*cont=*/false, /*int_flag=*/true);

        cdma_start_thread(0, 0x10000);
        wait_for_thread(0);

        bool ok = true;
        for (int i = 0; i < 256 && ok; ++i)
            if (hw_->sys_raw()[0x20000 + i] != static_cast<uint8_t>(i)) ok = false;
        ok ? print_pass("CDMA READ: 256 bytes correct (0x00–0xFF)")
           : print_fail("CDMA READ: data mismatch");
        dump_mem(hw_->sys_raw() + 0x20000, 32, "First 32 bytes read from xSPI");
    }

    // ── PB-2: CDMA PROGRAM ────────────────────────────────────────────────────
    void pb2_cdma_program() {
        banner("PB-2: CDMA PROGRAM (descriptor)", '-', 50);
        for (int i = 0; i < 128; ++i)
            hw_->sys_raw()[0x30000 + i] = static_cast<uint8_t>(0xA0 + (i & 0x0F));
        dump_mem(hw_->sys_raw() + 0x30000, 32, "Source in system memory");

        build_descriptor(hw_->sys_raw(),
            0x10040, 0,
            0x30000, 0x2000,
            acmd_command_type_e::PROGRAM, 127,
            0, true, false, true);

        cdma_start_thread(1, 0x10040);
        wait_for_thread(1);

        bool ok = true;
        for (int i = 0; i < 128 && ok; ++i)
            if (hw_->xspi_raw()[0x2000 + i] !=
                static_cast<uint8_t>(0xA0 + (i & 0x0F))) ok = false;
        ok ? print_pass("CDMA PROGRAM: 128 bytes verified in xSPI")
           : print_fail("CDMA PROGRAM: data mismatch in xSPI");
        dump_mem(hw_->xspi_raw() + 0x2000, 32, "Data in xSPI after PROGRAM");
    }

    // ── PB-3: CDMA ERASE SECTORS ─────────────────────────────────────────────
    void pb3_cdma_erase_sectors() {
        banner("PB-3: CDMA ERASE SECTORS (CMD=0x1000)", '-', 50);
        build_descriptor(hw_->sys_raw(),
            0x10080, 0, 0, 0x4000,
            acmd_command_type_e::ERASE_SECTORS, 7,
            0, false, false, false);

        cdma_start_thread(2, 0x10080);
        wait_for_thread(2);
        print_pass("CDMA ERASE SECTORS complete");
        // Read into local variable first to prevent [REG_RD] log from splitting the output line.
        uint32_t trd_after = read_reg(REG_TRD_STATUS);
        std::cout << "  TRD_STATUS after: " << hex32(trd_after) << "\n";
    }

    // ── PB-4: CDMA FULL CHIP ERASE ────────────────────────────────────────────
    void pb4_cdma_full_chip_erase() {
        banner("PB-4: CDMA FULL CHIP ERASE (CMD=0x1001)", '-', 50);
        build_descriptor(hw_->sys_raw(),
            0x100C0, 0, 0, 0,
            acmd_command_type_e::FULL_CHIP_ERASE, 0,
            0, false, false, false);

        cdma_start_thread(3, 0x100C0);
        wait_for_thread(3);
        print_pass("CDMA FULL CHIP ERASE complete");
    }

    // ── PB-5: CDMA DEVICE RESET ───────────────────────────────────────────────
    void pb5_cdma_device_reset() {
        banner("PB-5: CDMA DEVICE RESET (CMD=0x1100)", '-', 50);
        build_descriptor(hw_->sys_raw(),
            0x10100, 0, 0, 0,
            acmd_command_type_e::DEVICE_RESET, 0,
            0, false, false, false);

        cdma_start_thread(4, 0x10100);
        wait_for_thread(4);
        print_pass("CDMA DEVICE RESET complete");
    }

    // ── PB-6: CDMA Descriptor Chain ───────────────────────────────────────────
    void pb6_cdma_descriptor_chain() {
        banner("PB-6: CDMA Descriptor Chain (CONT, 2 chained READs)", '-', 50);
        for (int i = 0; i < 64; ++i) {
            hw_->xspi_raw()[0x5000 + i] = static_cast<uint8_t>(0x10 + i);
            hw_->xspi_raw()[0x5100 + i] = static_cast<uint8_t>(0x50 + i);
        }

        // desc-1 @ 0x11000: READ, CONT → desc-2 @ 0x11040
        build_descriptor(hw_->sys_raw(),
            0x11000, 0x11040,
            0x50000, 0x5000,
            acmd_command_type_e::READ, 63,
            0, true, /*cont=*/true, /*int_flag=*/false);

        // desc-2 @ 0x11040: READ, last, int_flag=1
        build_descriptor(hw_->sys_raw(),
            0x11040, 0,
            0x50100, 0x5100,
            acmd_command_type_e::READ, 63,
            0, true, /*cont=*/false, /*int_flag=*/true);

        std::cout << "  Chain: [0x11000] xSPI:0x5000→sys:0x50000"
                  << " CONT→ [0x11040] xSPI:0x5100→sys:0x50100\n";

        cdma_start_thread(5, 0x11000);
        wait_for_thread(5);

        bool ok1 = true, ok2 = true;
        for (int i = 0; i < 64; ++i) {
            if (hw_->sys_raw()[0x50000 + i] != static_cast<uint8_t>(0x10 + i)) ok1 = false;
            if (hw_->sys_raw()[0x50100 + i] != static_cast<uint8_t>(0x50 + i)) ok2 = false;
        }
        ok1 ? print_pass("Chain desc-1: 64 bytes correct (0x10..0x4F)")
            : print_fail("Chain desc-1: data mismatch");
        ok2 ? print_pass("Chain desc-2: 64 bytes correct (0x50..0x8F)")
            : print_fail("Chain desc-2: data mismatch");
        dump_mem(hw_->sys_raw() + 0x50000, 32, "Chain result-1");
        dump_mem(hw_->sys_raw() + 0x50100, 32, "Chain result-2");
    }

    // ── PB-7: CDMA Interrupt ──────────────────────────────────────────────────
    void pb7_cdma_interrupt() {
        banner("PB-7: CDMA Interrupt (int_flag=1)", '-', 50);
        for (int i = 0; i < 32; ++i)
            hw_->xspi_raw()[0x6000 + i] = static_cast<uint8_t>(0xC0 + i);

        build_descriptor(hw_->sys_raw(),
            0x11080, 0,
            0x60000, 0x6000,
            acmd_command_type_e::READ, 31,
            0, true, false, /*int_flag=*/true);

        // Clear any stale interrupt bits before the test
        write_reg(REG_TRD_COMP_INT, 0xFFFFu);
        wait(sc_time(20, SC_NS));

        cdma_start_thread(6, 0x11080);
        wait_for_thread(6);

        interrupt_in.read()
            ? print_pass("Interrupt line asserted after CDMA completion")
            : print_fail("Interrupt line NOT asserted");

        uint32_t comp = read_reg(REG_TRD_COMP_INT);
        (comp & (1u << 6))
            ? print_pass("TRD_COMP_INT[6] set, value=" + hex32(comp))
            : print_fail("TRD_COMP_INT[6] NOT set, value=" + hex32(comp));

        // W1C clear
        write_reg(REG_TRD_COMP_INT, (1u << 6));
        wait(sc_time(20, SC_NS));

        uint32_t comp2 = read_reg(REG_TRD_COMP_INT);
        ((comp2 & (1u << 6)) == 0)
            ? print_pass("TRD_COMP_INT[6] cleared (W1C)")
            : print_fail("TRD_COMP_INT[6] NOT cleared");
    }

    // ── PB-8: CMD_IGNORED – CDMA-H/I ─────────────────────────────────────────
    // Section 4.4.1: writing cmd_reg0 for a thread that is already busy must
    // be silently ignored and CMD_IGNORED must be set in intr_status (0x110).
    void pb8_cmd_ignored() {
        banner("PB-8: CMD_IGNORED (busy-thread write)", '-', 50);

        // Build a READ descriptor for thread 7.
        for (int i = 0; i < 64; ++i)
            hw_->xspi_raw()[0x7000 + i] = static_cast<uint8_t>(0xBB);

        build_descriptor(hw_->sys_raw(),
            0x12000, 0,
            0x70000, 0x7000,
            acmd_command_type_e::READ, 63,
            0, true, false, false);

        // Clear any stale CMD_IGNORED from a prior test.
        write_reg(REG_INTR_STATUS, CMD_IGNORED_BIT);
        wait(sc_time(5, SC_NS));

        // First trigger – starts thread 7 legitimately.
        cdma_start_thread(7, 0x12000);

        // NEW-2 FIX: Guarantee the second write hits the busy path by polling
        // TRD_STATUS[7] until the controller has actually set it.  Without this
        // poll, the second cdma_start_thread() could theoretically execute before
        // the execution thread has had a chance to process the first trigger and
        // mark TRD_STATUS[7], making the test timing-fragile.
        // The poll adds at most one 10-ns execution-thread cycle of latency.
        while (!(read_reg(REG_TRD_STATUS) & (1u << 7)))
            wait(sc_time(10, SC_NS));

        // Second trigger for the same thread while it is confirmed busy.
        // cdma_handle_trigger() must detect TRD_STATUS[7] set and call
        // signal_cmd_ignored() instead of starting a new operation.
        cdma_start_thread(7, 0x12000);

        // Allow the original (first) operation to finish normally.
        wait_for_thread(7);

        // Verify CMD_IGNORED was set in intr_status by the second write.
        // NEW-6 FIX: Use hex32() for clean, consistent formatting.
        uint32_t intr = read_reg(REG_INTR_STATUS);
        (intr & CMD_IGNORED_BIT)
            ? print_pass("CMD_IGNORED set in intr_status=" + hex32(REG_INTR_STATUS)
                         + " value=" + hex32(intr))
            : print_fail("CMD_IGNORED NOT set in intr_status="
                         + hex32(REG_INTR_STATUS) + " value=" + hex32(intr));

        // W1C clear of CMD_IGNORED.
        write_reg(REG_INTR_STATUS, CMD_IGNORED_BIT);
        wait(sc_time(10, SC_NS));
        uint32_t intr2 = read_reg(REG_INTR_STATUS);
        ((intr2 & CMD_IGNORED_BIT) == 0)
            ? print_pass("CMD_IGNORED cleared (W1C), intr_status=" + hex32(intr2))
            : print_fail("CMD_IGNORED NOT cleared, intr_status=" + hex32(intr2));
    }

    // ── PB-9: XIP-A – MB_XIP_EN on non-READ rejected ─────────────────────────
    // Table 4.7 note: MB_XIP_EN is only valid for READ commands.  A descriptor
    // with MB_XIP_EN set on a PROGRAM command must fail with DSC_ERROR.
    void pb9_xip_rejected() {
        banner("PB-9: XIP-A – MB_XIP_EN on PROGRAM rejected", '-', 50);

        // Build a PROGRAM descriptor with MB_XIP_EN set (invalid per spec)
        // Using a dedicated lambda so we can set mb_xip_en=true directly
        auto* sys = hw_->sys_raw();
        uint64_t desc_addr = 0x12040;
        auto* d = reinterpret_cast<acmd_command_descriptor_t*>(sys + desc_addr);
        d->next_pointer          = 0;
        d->system_memory_pointer = 0x80000;
        d->xspi_pointer          = 0x8000;
        d->command_type          = static_cast<uint16_t>(acmd_command_type_e::PROGRAM);
        d->command_counter       = 63;
        // Build flags with mb_xip_en=true (illegal on non-READ)
        d->command_flags = flags_build(0, false, false,
                                       /*mb_xip_en=*/true, false,
                                       false, false, false);
        d->status = 0;

        std::cout << "  Descriptor: PROGRAM + MB_XIP_EN (should be rejected)\n";

        // Thread 0 is IDLE (was used in PB-1 and completed long ago)
        cdma_start_thread(0, desc_addr);

        // Allow processing time; the descriptor fetch + validate path should
        // mark DSC_ERROR and set TRD_ERR_INT[0]
        wait(sc_time(500, SC_NS));

        // TRD_STATUS[0] should be clear (thread faulted back to IDLE)
        uint32_t trd = read_reg(REG_TRD_STATUS);
        ((trd & (1u << 0)) == 0)
            ? print_pass("TRD_STATUS[0] clear after XIP-A rejection")
            : print_fail("TRD_STATUS[0] still set, value=" + hex32(trd));

        // TRD_ERR_INT[0] should be set
        uint32_t err = read_reg(REG_TRD_ERR_INT);
        (err & (1u << 0))
            ? print_pass("TRD_ERR_INT[0] set after XIP-A rejection, value=" + hex32(err))
            : print_fail("TRD_ERR_INT[0] NOT set, value=" + hex32(err));

        // Clean up
        write_reg(REG_TRD_ERR_INT, 1u);
        wait(sc_time(10, SC_NS));
    }

    // =========================================================================
    //  Shared helpers
    // =========================================================================

    // Poll TRD_STATUS until thread tid's busy bit is clear (Section 4.4.3)
    void wait_for_thread(uint8_t tid) {
        std::cout << "  [POLL] Thread " << (int)tid << " ...\n";
        while (read_reg(REG_TRD_STATUS) & (1u << tid))
            wait(sc_time(100, SC_NS));
        std::cout << "  [POLL] Thread " << (int)tid
                  << " IDLE @ " << sc_time_stamp() << "\n";
    }

    // Read per-thread status via spec-defined indirect access (Section 4.4.3):
    //   write thread_id → REG_CMD_STATUS_PTR (0x040)
    //   read  status    ← REG_CMD_STATUS     (0x044)
    uint32_t read_thread_status(uint8_t tid) {
        write_reg(REG_CMD_STATUS_PTR, static_cast<uint32_t>(tid));
        return read_reg(REG_CMD_STATUS);
    }

    // Set descriptor address (cmd_reg2 LSB, cmd_reg3 MSB) and write cmd_reg0
    // to trigger a CDMA thread.  cmd_reg2/3 must be written before cmd_reg0.
    void cdma_start_thread(uint8_t thread_num, uint64_t desc_addr) {
        write_reg(REG_CMD_REG2, static_cast<uint32_t>(desc_addr & 0xFFFFFFFFu));
        write_reg(REG_CMD_REG3, static_cast<uint32_t>((desc_addr >> 32) & 0xFFFFFFFFu));
        uint32_t cmd0 = (CMD_REG0_MODE_CDMA << 30) |
                        (static_cast<uint32_t>(thread_num & 0x7u) << 24);
        write_reg(REG_CMD_REG0, cmd0);
    }

    // APB write via initiator socket
    void write_reg(uint32_t addr, uint32_t data) {
        tlm::tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        apb_init_socket->b_transport(trans, delay);
        wait(delay);
    }

    // APB read via initiator socket
    uint32_t read_reg(uint32_t addr) {
        uint32_t data = 0;
        tlm::tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        apb_init_socket->b_transport(trans, delay);
        wait(delay);
        return data;
    }

private:
    sc_event pio_done_;
};


// ============================================================================
//  SECTION 5 – Top-level testbench
// ============================================================================

SC_MODULE(testbench) {
    cdns_xspi_acmd_tlm* ctrl;
    memory_model*        mem;
    test_driver*         driver;
    sc_signal<bool>      interrupt_sig;

    SC_CTOR(testbench) {
        ctrl   = new cdns_xspi_acmd_tlm("xspi_ctrl");
        mem    = new memory_model("sys_mem");
        driver = new test_driver("test_driver");

        driver->apb_init_socket.bind(ctrl->apb_target_socket);
        ctrl->axi_master_socket.bind(mem->target_socket);
        ctrl->interrupt_out.bind(interrupt_sig);
        driver->interrupt_in.bind(interrupt_sig);

        // Pre-fill TLM memory for PIO tests
        mem->fill(0x001000u, 256u, 0xAAu);   // PA-1 READ  source
        mem->fill(0x030000u, 128u, 0x55u);   // PA-2 PROG  source

        std::cout << "[TB] Testbench constructed\n";
    }

    ~testbench() {
        delete driver;
        delete mem;
        delete ctrl;
    }
};


// ============================================================================
//  SECTION 6 – sc_main
// ============================================================================

int sc_main(int /*argc*/, char* /*argv*/[]) {
    banner("XSPI UNIFIED TESTBENCH – PIO + CDMA", '*', 62);
    std::cout << "  PIO  mode: cmd_reg0[31:30]=0b01\n"
              << "  CDMA mode: cmd_reg0[31:30]=0b00\n";

    testbench    tb("tb");
    MockHardware hw;

    tb.ctrl->set_dma_callbacks(
        [&hw](uint64_t s, uint64_t d, uint32_t n){ return hw.dma_read (s, d, n); },
        [&hw](uint64_t s, uint64_t d, uint32_t n){ return hw.dma_write(s, d, n); }
    );
    tb.driver->hw_ = &hw;

    std::cout << "\n[SC_MAIN] sc_start() – running...\n";
    sc_start();
    return 0;
}