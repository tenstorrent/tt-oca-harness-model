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
 * XSPI Unified Controller – Implementation
 * =========================================================================
 ***************************************************************************/

#include "cdns_xspi_acmd.h"

namespace mylibrary {

// =============================================================================
//  cdns_xspi_acmd_dma – non-SC CDMA engine
// =============================================================================

// ── Construction / Destruction ────────────────────────────────────────────────

cdns_xspi_acmd_dma::cdns_xspi_acmd_dma()
    : threads_(MAX_CDMA_THREADS),
      completion_interrupts_(0),
      error_interrupts_(0)
{
    // CDMA-C: MAX_CDMA_THREADS is now 8, matching the 3-bit TRD_NUM field (Table 4.3)
    for (uint8_t i = 0; i < MAX_CDMA_THREADS; ++i) {
        threads_[i].thread_number = i;
        threads_[i].state         = acmd_thread_state_e::IDLE;
        threads_[i].active        = false;
    }
    std::cout << "ACMD DMA: Initialized " << (int)MAX_CDMA_THREADS
              << " execution threads (TRD_NUM [26:24], max 8)" << std::endl;
}

cdns_xspi_acmd_dma::~cdns_xspi_acmd_dma()
{
    std::cout << "ACMD DMA: Shutting down Auto Command DMA controller" << std::endl;
}

// ── Mode helpers ──────────────────────────────────────────────────────────────

void cdns_xspi_acmd_dma::enable_acmd_mode()
{
    std::cout << "\n--- Enabling ACMD Mode ---" << std::endl;
    if (!write_register_ || !read_register_) {
        std::cerr << "ACMD DMA ERROR: Register callbacks not set!" << std::endl;
        return;
    }
    uint32_t ctrl_config = read_register_(REG_CTRL_CONFIG);
    ctrl_config = (ctrl_config & ~WORK_MODE_MASK) | WORK_MODE_ACMD;
    write_register_(REG_CTRL_CONFIG, ctrl_config);
    std::cout << "ACMD mode enabled (ctrl_config=0x"
              << std::hex << ctrl_config << std::dec << ")" << std::endl;
}

void cdns_xspi_acmd_dma::disable_acmd_mode()
{
    std::cout << "\n--- Disabling ACMD Mode ---" << std::endl;
    if (!write_register_ || !read_register_) {
        std::cerr << "ACMD DMA ERROR: Register callbacks not set!" << std::endl;
        return;
    }
    uint32_t ctrl_config = read_register_(REG_CTRL_CONFIG) & ~WORK_MODE_MASK;
    write_register_(REG_CTRL_CONFIG, ctrl_config);
    std::cout << "ACMD mode disabled" << std::endl;
}

bool cdns_xspi_acmd_dma::is_acmd_mode_enabled() const
{
    if (!read_register_) return false;
    return ((read_register_(REG_CTRL_CONFIG) & WORK_MODE_MASK) == WORK_MODE_ACMD);
}

// ── Thread management ─────────────────────────────────────────────────────────

bool cdns_xspi_acmd_dma::start_thread(uint8_t thread_num, uint64_t descriptor_head_addr)
{
    std::cout << "\n========================================" << std::endl;
    std::cout << "Starting Thread " << (int)thread_num << std::endl;
    std::cout << "========================================" << std::endl;

    if (thread_num >= MAX_CDMA_THREADS) {
        std::cerr << "ERROR: Invalid thread number " << (int)thread_num << std::endl;
        return false;
    }
    if ((descriptor_head_addr & 0x3F) != 0) {
        std::cerr << "ERROR: Descriptor address 0x"
                  << std::hex << descriptor_head_addr
                  << " not 64-byte aligned" << std::dec << std::endl;
        return false;
    }

    // Section 4.4.1: verify controller not busy before starting
    if (check_controller_busy()) {
        std::cerr << "ERROR: Controller is busy, cannot start thread" << std::endl;
        return false;
    }
    std::cout << "\u2713 Controller is ready" << std::endl;

    if (!check_thread_idle(thread_num)) {
        std::cerr << "ERROR: Thread " << (int)thread_num << " is not IDLE" << std::endl;
        return false;
    }
    std::cout << "\u2713 Thread " << (int)thread_num << " is IDLE" << std::endl;

    acmd_dma_thread_t& thread = threads_[thread_num];
    thread.descriptor_head_address = descriptor_head_addr;
    thread.state  = acmd_thread_state_e::FETCHING_DESCRIPTOR;
    thread.active = true;

    // Write descriptor address: cmd_reg2 (LSB) then cmd_reg3 (MSB)
    // per Table 4.4/4.5 — must precede cmd_reg0 write
    uint32_t addr_lower = static_cast<uint32_t>(descriptor_head_addr & 0xFFFFFFFFu);
    uint32_t addr_upper = static_cast<uint32_t>((descriptor_head_addr >> 32) & 0xFFFFFFFFu);
    write_cmd_reg2(addr_lower);
    write_cmd_reg3(addr_upper);

    // cmd_reg0: bits[31:30]=0b00 (CDMA), bits[26:24]=thread_num (3-bit TRD_NUM)
    uint32_t cmd_reg0 = (CMD_REG0_MODE_CDMA << 30) |
                        (static_cast<uint32_t>(thread_num & 0x7u) << 24);
    std::cout << "  cmd_reg0 = 0x" << std::hex << cmd_reg0 << std::dec
              << "  (CDMA, TRD_NUM=" << (int)thread_num << ")" << std::endl;
    write_cmd_reg0(cmd_reg0);

    std::cout << "\u2713 Thread " << (int)thread_num << " started" << std::endl;
    std::cout << "========================================\n" << std::endl;
    return true;
}

acmd_thread_state_e cdns_xspi_acmd_dma::get_thread_state(uint8_t thread_num) const
{
    if (thread_num >= MAX_CDMA_THREADS) return acmd_thread_state_e::ERROR;
    return threads_[thread_num].state;
}

bool cdns_xspi_acmd_dma::is_thread_busy(uint8_t thread_num) const
{
    if (thread_num >= MAX_CDMA_THREADS) return false;
    const acmd_dma_thread_t& t = threads_[thread_num];
    return t.active && (t.state != acmd_thread_state_e::IDLE);
}

// ── Descriptor operations ─────────────────────────────────────────────────────

bool cdns_xspi_acmd_dma::fetch_descriptor(uint64_t address,
                                           acmd_command_descriptor_t& descriptor)
{
    if (!dma_read_callback_) {
        std::cerr << "ACMD DMA ERROR: DMA read callback not set!" << std::endl;
        return false;
    }
    // Spec: address pointers must be 64-bit boundary aligned
    if ((address & 0x3F) != 0) {
        std::cerr << "ACMD DMA ERROR: Descriptor address 0x"
                  << std::hex << address
                  << " not 64-byte aligned" << std::dec << std::endl;
        return false;
    }
    std::cout << "\n--- Fetching Descriptor @ 0x"
              << std::hex << address << std::dec << " ---" << std::endl;

    uint8_t desc_buffer[64];
    memset(desc_buffer, 0, 64);
    bool success = dma_read_callback_(address,
                                      reinterpret_cast<uint64_t>(desc_buffer),
                                      64);
    if (!success) {
        std::cerr << "ERROR: Failed to fetch descriptor from 0x"
                  << std::hex << address << std::dec << std::endl;
        return false;
    }

    // Portable little-endian extraction (avoids strict-aliasing UB)
    auto le64 = [](const uint8_t* p) -> uint64_t {
        uint64_t v = 0;
        for (int b = 7; b >= 0; --b) v = (v << 8) | p[b];
        return v;
    };

    uint64_t w[8];
    for (int i = 0; i < 8; ++i) w[i] = le64(desc_buffer + i * 8);

    descriptor.next_pointer          = w[0];
    descriptor.system_memory_pointer = w[1];
    descriptor.xspi_pointer          = w[2];
    descriptor.reserved_3            = w[3];
    descriptor.command_type          = static_cast<uint16_t>(w[4] & 0xFFFFu);
    descriptor.command_flags         = static_cast<uint16_t>((w[4] >> 16) & 0xFFFFu);
    descriptor.command_counter       = static_cast<uint16_t>((w[4] >> 32) & 0xFFFFu);
    descriptor.reserved_4_upper      = static_cast<uint16_t>((w[4] >> 48) & 0xFFFFu);
    descriptor.status                = static_cast<uint32_t>(w[5] & 0xFFFFFFFFu);
    descriptor.reserved_5_upper      = static_cast<uint32_t>((w[5] >> 32) & 0xFFFFFFFFu);

    std::cout << "  Next Ptr:     0x" << std::hex << descriptor.next_pointer    << std::dec << "\n"
              << "  Sys Mem Ptr:  0x" << std::hex << descriptor.system_memory_pointer << std::dec << "\n"
              << "  xSPI Ptr:     0x" << std::hex << descriptor.xspi_pointer    << std::dec << "\n"
              << "  Cmd Type:     0x" << std::hex << descriptor.command_type;
    switch (static_cast<acmd_command_type_e>(descriptor.command_type)) {
        case acmd_command_type_e::READ:           std::cout << " (READ)";           break;
        case acmd_command_type_e::PROGRAM:        std::cout << " (PROGRAM)";        break;
        case acmd_command_type_e::ERASE_SECTORS:  std::cout << " (ERASE_SECTORS)";  break;
        case acmd_command_type_e::FULL_CHIP_ERASE:std::cout << " (FULL_CHIP_ERASE)";break;
        case acmd_command_type_e::DEVICE_RESET:   std::cout << " (DEVICE_RESET)";   break;
        case acmd_command_type_e::JEDEC_RESET:    std::cout << " (JEDEC_RESET)";    break;
        default:                                   std::cout << " (UNKNOWN)";        break;
    }
    std::cout << std::dec << "\n"
              << "  Cmd Flags:    0x" << std::hex << descriptor.command_flags << std::dec
              << "  DMA_SEL="  << flags_dma_sel(descriptor.command_flags)
              << " CONT="      << flags_cont(descriptor.command_flags)
              << " INT="       << flags_int_flag(descriptor.command_flags)
              << " BANK="      << (int)flags_bank(descriptor.command_flags) << "\n"
              << "  Cmd Counter: " << descriptor.command_counter
              << " (count=" << (descriptor.command_counter + 1) << ")\n";

    if (!validate_descriptor(descriptor)) {
        std::cerr << "ERROR: Descriptor validation failed" << std::endl;
        // DSC_ERROR set by caller in FETCHING_DESCRIPTOR state
        return false;
    }
    std::cout << "\u2713 Descriptor validated" << std::endl;
    return true;
}

bool cdns_xspi_acmd_dma::validate_descriptor(const acmd_command_descriptor_t& descriptor)
{
    // Validate command type
    switch (static_cast<acmd_command_type_e>(descriptor.command_type)) {
        case acmd_command_type_e::ERASE_SECTORS:
        case acmd_command_type_e::FULL_CHIP_ERASE:
        case acmd_command_type_e::DEVICE_RESET:
        case acmd_command_type_e::JEDEC_RESET:
        case acmd_command_type_e::PROGRAM:
        case acmd_command_type_e::READ:
            break;
        default:
            std::cerr << "  \u2717 Invalid command type: 0x"
                      << std::hex << descriptor.command_type << std::dec << std::endl;
            return false;
    }

    // XIP-A FIX: Table 4.7 note – MB_XIP_EN is only valid for READ commands.
    // Reject any non-READ descriptor that attempts to set MB_XIP_EN.
    if (flags_mb_xip_en(descriptor.command_flags)) {
        acmd_command_type_e ct = static_cast<acmd_command_type_e>(descriptor.command_type);
        if (ct != acmd_command_type_e::READ) {
            std::cerr << "  \u2717 MB_XIP_EN set on non-READ command type 0x"
                      << std::hex << descriptor.command_type << std::dec
                      << " (XIP-A: only valid for READ)" << std::endl;
            return false;
        }
    }

    // Spec: next_pointer must be 64-byte aligned if non-zero
    if (descriptor.next_pointer != 0 && (descriptor.next_pointer & 0x3F) != 0) {
        std::cerr << "  \u2717 Next pointer 0x"
                  << std::hex << descriptor.next_pointer
                  << " not 64-byte aligned" << std::dec << std::endl;
        return false;
    }

    // CONT flag must be consistent with next_pointer
    bool cont = flags_cont(descriptor.command_flags);
    if (cont && descriptor.next_pointer == 0) {
        std::cerr << "  \u2717 CONT set but next_pointer is 0" << std::endl;
        return false;
    }
    if (!cont && descriptor.next_pointer != 0) {
        std::cerr << "  \u26a0 Warning: CONT not set but next_pointer is non-zero" << std::endl;
    }

    return true;
}

bool cdns_xspi_acmd_dma::update_descriptor_status(uint64_t address, uint32_t status)
{
    if (!dma_write_callback_) {
        std::cerr << "ACMD DMA ERROR: DMA write callback not set!" << std::endl;
        return false;
    }
    // Status field is in Word 5 lower 32 bits = byte offset 40 from descriptor base
    // (Table 4.6: words 0-3 = 32 bytes, word 4 = 8 bytes, so word 5 starts at byte 40)
    uint64_t status_address = address + 40u;

    uint8_t status_buffer[4];
    memcpy(status_buffer, &status, 4);
    bool ok = dma_write_callback_(reinterpret_cast<uint64_t>(status_buffer),
                                  status_address, 4);
    if (!ok) {
        std::cerr << "ERROR: Failed to write descriptor status to 0x"
                  << std::hex << status_address << std::dec << std::endl;
        return false;
    }
    std::cout << "  Descriptor status writeback: 0x" << std::hex << status << std::dec;
    if (status & cdma_status_bits::COMPLETE)    std::cout << " COMPLETE";
    if (status & cdma_status_bits::FAIL)        std::cout << " FAIL";
    if (status & cdma_status_bits::DSC_ERROR)   std::cout << " DSC_ERROR";
    if (status & cdma_status_bits::BUS_ERROR)   std::cout << " BUS_ERROR";
    if (status & cdma_status_bits::DEVICE_ERROR)std::cout << " DEVICE_ERROR";
    std::cout << std::endl;
    return true;
}

// ── Command execution ─────────────────────────────────────────────────────────

bool cdns_xspi_acmd_dma::execute_descriptor(acmd_dma_thread_t& thread)
{
    acmd_command_descriptor_t& desc = thread.current_desc;
    bool success = false;

    switch (static_cast<acmd_command_type_e>(desc.command_type)) {
        case acmd_command_type_e::READ:
            success = execute_read_command(desc);    break;
        case acmd_command_type_e::PROGRAM:
            success = execute_program_command(desc); break;
        case acmd_command_type_e::ERASE_SECTORS:
        case acmd_command_type_e::FULL_CHIP_ERASE:
            success = execute_erase_command(desc);   break;
        case acmd_command_type_e::DEVICE_RESET:
        case acmd_command_type_e::JEDEC_RESET:
            success = execute_reset_command(desc);   break;
        default:
            std::cerr << "ERROR: Unknown command type 0x"
                      << std::hex << desc.command_type << std::dec << std::endl;
            desc.status |= cdma_status_bits::DSC_ERROR;
            success = false;
    }

    // COMPLETE is always set (even on failure), per spec Table 4.9
    desc.status |= cdma_status_bits::COMPLETE;
    if (!success) desc.status |= cdma_status_bits::FAIL;

    update_descriptor_status(thread.descriptor_head_address, desc.status);
    return success;
}

bool cdns_xspi_acmd_dma::execute_read_command(acmd_command_descriptor_t& desc)
{
    uint32_t byte_count = static_cast<uint32_t>(desc.command_counter) + 1u;
    bool use_master = flags_dma_sel(desc.command_flags);

    std::cout << "[CDMA_READ] xSPI=0x" << std::hex << desc.xspi_pointer
              << " sys=0x" << desc.system_memory_pointer
              << std::dec << " bytes=" << byte_count
              << " iface=" << (use_master ? "Master" : "Slave")
              << " bank=" << (int)flags_bank(desc.command_flags) << std::endl;

    bool ok = dma_read_from_xspi(desc.xspi_pointer,
                                  desc.system_memory_pointer,
                                  byte_count, use_master);
    // DEFECT CDMA-8 FIX: use BUS_ERROR (bit 1), not DEVICE_ERROR (bit 4)
    if (!ok) desc.status |= cdma_status_bits::BUS_ERROR;
    return ok;
}

bool cdns_xspi_acmd_dma::execute_program_command(acmd_command_descriptor_t& desc)
{
    uint32_t byte_count = static_cast<uint32_t>(desc.command_counter) + 1u;
    bool use_master = flags_dma_sel(desc.command_flags);

    std::cout << "[CDMA_PROG] sys=0x" << std::hex << desc.system_memory_pointer
              << " xSPI=0x" << desc.xspi_pointer
              << std::dec << " bytes=" << byte_count
              << " iface=" << (use_master ? "Master" : "Slave")
              << " bank=" << (int)flags_bank(desc.command_flags) << std::endl;

    bool ok = dma_write_to_xspi(desc.system_memory_pointer,
                                 desc.xspi_pointer,
                                 byte_count, use_master);
    // DEFECT CDMA-8 FIX: use BUS_ERROR (bit 1), not DEVICE_ERROR (bit 4)
    if (!ok) desc.status |= cdma_status_bits::BUS_ERROR;
    return ok;
}

bool cdns_xspi_acmd_dma::execute_erase_command(acmd_command_descriptor_t& desc)
{
    acmd_command_type_e ct = static_cast<acmd_command_type_e>(desc.command_type);
    if (ct == acmd_command_type_e::FULL_CHIP_ERASE) {
        std::cout << "[CDMA_ERASE] Full Chip Erase"
                  << " bank=" << (int)flags_bank(desc.command_flags) << std::endl;
    } else {
        // Sector erase: command_counter = number of sectors to erase (Table 4.7)
        std::cout << "[CDMA_ERASE] Sector Erase xSPI=0x"
                  << std::hex << desc.xspi_pointer << std::dec
                  << " sectors=" << desc.command_counter
                  << " bank=" << (int)flags_bank(desc.command_flags) << std::endl;
    }
    return true;
}

bool cdns_xspi_acmd_dma::execute_reset_command(acmd_command_descriptor_t& desc)
{
    acmd_command_type_e ct = static_cast<acmd_command_type_e>(desc.command_type);
    std::cout << "[CDMA_RESET] "
              << (ct == acmd_command_type_e::JEDEC_RESET ? "JEDEC Reset (0x1101)"
                                                         : "Device Reset (0x1100)")
              << " bank=" << (int)flags_bank(desc.command_flags) << std::endl;
    return true;
}

// ── DMA transfer operations ───────────────────────────────────────────────────

bool cdns_xspi_acmd_dma::dma_read_from_xspi(uint64_t xspi_addr, uint64_t sys_addr,
                                              uint32_t byte_count, bool use_dma_master)
{
    std::cout << "[DMA_RD] xSPI=0x" << std::hex << xspi_addr
              << " → sys=0x" << sys_addr << std::dec
              << " size=" << byte_count
              << " [" << (use_dma_master ? "Master" : "Slave") << "]" << std::endl;
    if (!dma_read_callback_) {
        std::cerr << "ERROR: DMA read callback not set!" << std::endl;
        return false;
    }
    return dma_read_callback_(xspi_addr, sys_addr, byte_count);
}

bool cdns_xspi_acmd_dma::dma_write_to_xspi(uint64_t sys_addr, uint64_t xspi_addr,
                                             uint32_t byte_count, bool use_dma_master)
{
    std::cout << "[DMA_WR] sys=0x" << std::hex << sys_addr
              << " → xSPI=0x" << xspi_addr << std::dec
              << " size=" << byte_count
              << " [" << (use_dma_master ? "Master" : "Slave") << "]" << std::endl;
    if (!dma_write_callback_) {
        std::cerr << "ERROR: DMA write callback not set!" << std::endl;
        return false;
    }
    return dma_write_callback_(sys_addr, xspi_addr, byte_count);
}

// ── Callback registration ─────────────────────────────────────────────────────

void cdns_xspi_acmd_dma::set_register_callbacks(
    std::function<void(uint32_t, uint32_t)> write_reg,
    std::function<uint32_t(uint32_t)>       read_reg)
{
    write_register_ = write_reg;
    read_register_  = read_reg;
    std::cout << "ACMD DMA: Register callbacks set" << std::endl;
}

void cdns_xspi_acmd_dma::set_dma_callbacks(
    std::function<bool(uint64_t, uint64_t, uint32_t)> dma_read,
    std::function<bool(uint64_t, uint64_t, uint32_t)> dma_write)
{
    dma_read_callback_  = dma_read;
    dma_write_callback_ = dma_write;
    std::cout << "ACMD DMA: DMA callbacks set" << std::endl;
}

// ── Interrupt / status management ─────────────────────────────────────────────

uint16_t cdns_xspi_acmd_dma::check_completion_interrupts()
{
    if (read_register_)
        completion_interrupts_ = static_cast<uint16_t>(
            read_register_(REG_TRD_COMP_INT) & 0xFFFFu);
    return completion_interrupts_;
}

uint16_t cdns_xspi_acmd_dma::check_error_interrupts()
{
    if (read_register_)
        error_interrupts_ = static_cast<uint16_t>(
            read_register_(REG_TRD_ERR_INT) & 0xFFFFu);
    return error_interrupts_;
}

void cdns_xspi_acmd_dma::clear_completion_interrupt(uint8_t thread_num)
{
    if (thread_num >= MAX_CDMA_THREADS) return;
    uint32_t mask = (1u << thread_num);
    if (write_register_) write_register_(REG_TRD_COMP_INT, mask);  // W1C
    completion_interrupts_ &= ~static_cast<uint16_t>(mask);
}

void cdns_xspi_acmd_dma::clear_error_interrupt(uint8_t thread_num)
{
    if (thread_num >= MAX_CDMA_THREADS) return;
    uint32_t mask = (1u << thread_num);
    if (write_register_) write_register_(REG_TRD_ERR_INT, mask);   // W1C
    error_interrupts_ &= ~static_cast<uint16_t>(mask);
}

// ── Main processing loop ──────────────────────────────────────────────────────

void cdns_xspi_acmd_dma::process_threads()
{
    for (auto& thread : threads_) {
        if (!thread.active) continue;

        switch (thread.state) {

            case acmd_thread_state_e::FETCHING_DESCRIPTOR: {
                bool ok = fetch_descriptor(thread.descriptor_head_address,
                                           thread.current_desc);
                if (ok) {
                    thread.state = acmd_thread_state_e::EXECUTING_COMMAND;
                } else {
                    thread.current_desc.status |= cdma_status_bits::DSC_ERROR;
                    thread.state = acmd_thread_state_e::ERROR;
                    error_interrupts_ |= static_cast<uint16_t>(1u << thread.thread_number);
                }
                break;
            }

            case acmd_thread_state_e::EXECUTING_COMMAND: {
                bool ok = execute_descriptor(thread);
                if (ok) {
                    thread.state = acmd_thread_state_e::UPDATING_STATUS;
                } else {
                    thread.state = acmd_thread_state_e::ERROR;
                    error_interrupts_ |= static_cast<uint16_t>(1u << thread.thread_number);
                }
                break;
            }

            case acmd_thread_state_e::UPDATING_STATUS: {
                bool cont     = flags_cont    (thread.current_desc.command_flags);
                bool int_flag = flags_int_flag(thread.current_desc.command_flags);
                uint64_t next = thread.current_desc.next_pointer;

                if (cont && next != 0) {
                    // Chain to next descriptor
                    std::cout << "Thread " << (int)thread.thread_number
                              << " chaining to 0x" << std::hex << next << std::dec
                              << std::endl;
                    thread.descriptor_head_address = next;
                    thread.state = acmd_thread_state_e::FETCHING_DESCRIPTOR;
                } else {
                    // End of chain
                    thread.state = acmd_thread_state_e::COMPLETE;
                    if (int_flag) {
                        completion_interrupts_ |=
                            static_cast<uint16_t>(1u << thread.thread_number);
                    }
                }
                break;
            }

            case acmd_thread_state_e::COMPLETE:
                thread.active = false;
                thread.state  = acmd_thread_state_e::IDLE;
                std::cout << "Thread " << (int)thread.thread_number
                          << " COMPLETE → IDLE" << std::endl;
                break;

            case acmd_thread_state_e::ERROR:
                thread.active = false;
                thread.state  = acmd_thread_state_e::IDLE;
                std::cerr << "Thread " << (int)thread.thread_number
                          << " ERROR → IDLE" << std::endl;
                break;

            default:
                break;
        }
    }
}

bool cdns_xspi_acmd_dma::wait_for_thread_completion(uint8_t thread_num,
                                                      uint32_t timeout_ms)
{
    if (thread_num >= MAX_CDMA_THREADS) return false;
    auto start = std::chrono::steady_clock::now();
    while (is_thread_busy(thread_num)) {
        process_threads();
        if (check_completion_interrupts() & (1u << thread_num)) {
            clear_completion_interrupt(thread_num);
            return true;
        }
        if (check_error_interrupts() & (1u << thread_num)) {
            clear_error_interrupt(thread_num);
            return false;
        }
        if (timeout_ms > 0) {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (static_cast<uint32_t>(ms) >= timeout_ms) return false;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    return (get_thread_state(thread_num) == acmd_thread_state_e::IDLE);
}

// ── Private register helpers ──────────────────────────────────────────────────

void cdns_xspi_acmd_dma::write_cmd_reg0(uint32_t v)
{
    if (write_register_) write_register_(REG_CMD_REG0, v);
}
void cdns_xspi_acmd_dma::write_cmd_reg2(uint32_t v)
{
    if (write_register_) write_register_(REG_CMD_REG2, v);
}
void cdns_xspi_acmd_dma::write_cmd_reg3(uint32_t v)
{
    if (write_register_) write_register_(REG_CMD_REG3, v);
}
uint32_t cdns_xspi_acmd_dma::read_ctrl_status()
{
    return read_register_ ? read_register_(REG_CTRL_STATUS) : 0u;
}
uint32_t cdns_xspi_acmd_dma::read_thread_status()
{
    return read_register_ ? read_register_(REG_TRD_STATUS) : 0u;
}

bool cdns_xspi_acmd_dma::check_controller_busy()
{
    if (!read_register_) { return true; }
    int timeout = 10000;
    while (true) {
        if ((read_ctrl_status() & CTRL_BUSY_BIT) == 0) return false;
        if (--timeout <= 0) {
            std::cerr << "ERROR: Controller busy timeout" << std::endl;
            return true;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
}

// DEFECT CDMA-W1 FIX: 1-bit-per-thread (matches TLM module's 1u << thread_num usage)
bool cdns_xspi_acmd_dma::check_thread_idle(uint8_t thread_num)
{
    if (thread_num >= MAX_CDMA_THREADS || !read_register_) return false;
    uint32_t trd_status = read_thread_status();
    if (trd_status & (1u << thread_num)) {
        std::cerr << "Thread " << (int)thread_num << " not IDLE (TRD_STATUS bit set)" << std::endl;
        return false;
    }
    return true;
}


// =============================================================================
//  cdns_xspi_acmd_tlm – SystemC module
// =============================================================================

// ── Constructor / Destructor ──────────────────────────────────────────────────

cdns_xspi_acmd_tlm::cdns_xspi_acmd_tlm(sc_core::sc_module_name name)
    : sc_core::sc_module(name),
      apb_target_socket("apb_target_socket"),
      axi_master_socket("axi_master_socket"),
      interrupt_out("interrupt_out")
{
    apb_target_socket.register_b_transport(
        this, &cdns_xspi_acmd_tlm::apb_b_transport);

    SC_THREAD(command_execution_thread);

    init_registers();
    pio_init_threads();

    // Wire CDMA engine register callbacks into this module's register file
    acmd_dma_.set_register_callbacks(
        [this](uint32_t off, uint32_t val) { reg_cb_write(off, val); },
        [this](uint32_t off)               { return reg_cb_read(off); }
    );

    std::cout << "[XSPI_ACMD] Unified controller instantiated (PIO + CDMA)" << std::endl;
}

cdns_xspi_acmd_tlm::~cdns_xspi_acmd_tlm()
{
    std::cout << "[XSPI_ACMD] Unified controller destroyed" << std::endl;
}

void cdns_xspi_acmd_tlm::set_dma_callbacks(
    std::function<bool(uint64_t, uint64_t, uint32_t)> dma_read,
    std::function<bool(uint64_t, uint64_t, uint32_t)> dma_write)
{
    acmd_dma_.set_dma_callbacks(dma_read, dma_write);
}

// ── Initialisation ────────────────────────────────────────────────────────────

void cdns_xspi_acmd_tlm::init_registers()
{
    registers_[REG_CTRL_CONFIG]    = 0;
    registers_[REG_CTRL_STATUS]    = 0;
    registers_[REG_TRD_STATUS]     = 0;
    registers_[REG_TRD_COMP_INT]   = 0;
    registers_[REG_TRD_ERR_INT]    = 0;
    registers_[REG_INTR_STATUS]    = 0;   // CDMA-I: intr_status (0x110) – CMD_IGNORED at bit 0
    registers_[REG_CMD_STATUS_PTR] = 0;   // cmd_status_ptr thread selector
    registers_[REG_CMD_STATUS]     = 0;   // cmd_status (read-only, driven by register_read)
}

void cdns_xspi_acmd_tlm::pio_init_threads()
{
    // PIO TRD_NUM [26:24] = 3 bits → maximum 8 threads (MAX_THREADS = 8)
    pio_threads_.resize(MAX_THREADS);
    for (uint8_t i = 0; i < MAX_THREADS; ++i) {
        pio_threads_[i].thread_id = i;
        pio_threads_[i].state     = pio_thread_state_e::IDLE;
        thread_status_[i]         = pio_status_reg_t();
    }
}

// ── Register callbacks exposed to CDMA engine ─────────────────────────────────

void cdns_xspi_acmd_tlm::reg_cb_write(uint32_t off, uint32_t val)
{
    registers_[off] = val;
}

uint32_t cdns_xspi_acmd_tlm::reg_cb_read(uint32_t off)
{
    return registers_[off];
}

// ── CTRL_BUSY helpers (DEFECT CDMA-9 FIX) ────────────────────────────────────

void cdns_xspi_acmd_tlm::set_ctrl_busy(bool busy)
{
    if (busy)
        registers_[REG_CTRL_STATUS] |=  CTRL_BUSY_BIT;
    else
        registers_[REG_CTRL_STATUS] &= ~CTRL_BUSY_BIT;
}

bool cdns_xspi_acmd_tlm::any_thread_active() const
{
    // PIO threads
    for (const auto& t : pio_threads_)
        if (t.state == pio_thread_state_e::BUSY) return true;
    // CDMA threads
    for (const auto& t : acmd_dma_.threads_)
        if (t.active) return true;
    return false;
}

// CDMA-I: signal_cmd_ignored – sets CMD_IGNORED bit in intr_status (0x110)
// Called by cdma_handle_trigger when the target thread is already busy.
// Section 4.4.1: "If a thread is busy and cmd_reg0 is written, the command
// is ignored and the CMD_IGNORED flag is set in intr_status."
void cdns_xspi_acmd_tlm::signal_cmd_ignored()
{
    registers_[REG_INTR_STATUS] |= CMD_IGNORED_BIT;
    std::cerr << "[CDMA_TRIGGER] CMD_IGNORED set in intr_status (0x"
              << std::hex << REG_INTR_STATUS << std::dec << ")\n";
    // NEW-7 NOTE: Whether intr_status (0x110) asserts the interrupt output pin
    // could not be confirmed from the provided spec screenshots.  It is modelled
    // here as a status-only register that does NOT drive the interrupt line;
    // only TRD_COMP_INT (0x120) and TRD_ERR_INT (0x130) drive interrupt_out.
    // Verify this assumption against the full interrupt-routing description in
    // the spec if the document is available.
}

// ── APB TLM transport ─────────────────────────────────────────────────────────

void cdns_xspi_acmd_tlm::apb_b_transport(tlm::tlm_generic_payload& trans,
                                          sc_core::sc_time& delay)
{
    if (trans.get_data_length() != 4) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    reg_mutex_.lock();
    uint32_t addr = static_cast<uint32_t>(trans.get_address());
    auto*    ptr  = trans.get_data_ptr();

    if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
        register_write(addr, *reinterpret_cast<uint32_t*>(ptr));
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    } else if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        *reinterpret_cast<uint32_t*>(ptr) = register_read(addr);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    } else {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
    }
    reg_mutex_.unlock();
    delay += sc_core::sc_time(10, sc_core::SC_NS);
}

// ── Register access ───────────────────────────────────────────────────────────

void cdns_xspi_acmd_tlm::register_write(uint32_t addr, uint32_t data)
{
    std::cout << "[REG_WR] 0x" << std::hex << std::setw(3) << std::setfill('0')
              << addr << " = 0x" << std::setw(8) << data << std::dec << std::endl;

    // W1C registers: clear the bits indicated by the written value
    if (addr == REG_TRD_COMP_INT || addr == REG_TRD_ERR_INT) {
        registers_[addr] &= ~data;
        // Interrupt line de-assertion is handled in the execution thread
        return;
    }

    // CDMA-I: intr_status (0x110) is also W1C – writing 1 clears CMD_IGNORED
    if (addr == REG_INTR_STATUS) {
        registers_[REG_INTR_STATUS] &= ~data;
        return;
    }

    registers_[addr] = data;

    // Writing cmd_reg0 triggers command execution (spec: "write access to this
    // register triggers the execution of the programmed operation")
    if (addr == REG_CMD_REG0) {
        handle_trigger(data);
    }
}

uint32_t cdns_xspi_acmd_tlm::register_read(uint32_t addr)
{
    uint32_t data;

    // DEFECT PIO-5 FIX: Section 4.4.3 indirect status access
    // Reading REG_CMD_STATUS (0x044) returns the status of the thread
    // whose ID was last written to REG_CMD_STATUS_PTR (0x040).
    // Selector is clamped to valid range [0, MAX_THREADS-1] (RESIDUAL-3 fix).
    if (addr == REG_CMD_STATUS) {
        uint8_t sel = static_cast<uint8_t>(
            registers_[REG_CMD_STATUS_PTR] & static_cast<uint32_t>(MAX_THREADS - 1u));
        data = thread_status_[sel].to_reg();
    } else {
        data = registers_[addr];
    }

    std::cout << "[REG_RD] 0x" << std::hex << std::setw(3) << std::setfill('0')
              << addr << " = 0x" << std::setw(8) << data << std::dec << std::endl;
    return data;
}

// ── Trigger dispatch ──────────────────────────────────────────────────────────

void cdns_xspi_acmd_tlm::handle_trigger(uint32_t cmd_reg0)
{
    uint32_t mode_bits = (cmd_reg0 >> 30) & 0x3u;

    if (mode_bits == CMD_REG0_MODE_PIO) {
        pio_handle_trigger(cmd_reg0);
    } else if (mode_bits == CMD_REG0_MODE_CDMA) {
        cdma_handle_trigger(cmd_reg0);
    } else {
        std::cerr << "[TRIGGER] Unknown mode bits 0x"
                  << std::hex << mode_bits << std::dec << std::endl;
    }
}

// ── PIO path  (Section 4.4.2) ─────────────────────────────────────────────────

void cdns_xspi_acmd_tlm::pio_handle_trigger(uint32_t cmd_reg0)
{
    // DEFECT PIO-3 FIX: PIO requires global ACMD work_mode (2'b11) per ctrl_config[6:5]
    if ((registers_[REG_CTRL_CONFIG] & WORK_MODE_MASK) != WORK_MODE_ACMD) {
        std::cerr << "[PIO_TRIGGER] ERROR: ctrl_config not in ACMD work_mode (2'b11) (0x"
                  << std::hex << registers_[REG_CTRL_CONFIG] << std::dec << ")\n";
        return;
    }

    // CTRL-A NOTE: The spec (Section 4.4.1) does not explicitly mandate that
    // software must poll ctrl_busy before each PIO cmd_reg0 write.  This guard
    // is retained as an implementation-defined safeguard to prevent a second
    // write while the controller is still processing a prior command.  If a
    // strictly spec-only model is required, remove this block.
    if (registers_[REG_CTRL_STATUS] & CTRL_BUSY_BIT) {
        std::cerr << "[PIO_TRIGGER] WARNING: ctrl_busy set, ignoring cmd_reg0 write\n";
        return;
    }

    // PIO-D FIX: Mask reserved bits [29:27] and [23] before decoding cmd_reg0
    // (Table 4.10).  Hardware must ignore writes to reserved fields; masking
    // here prevents reserved bits from contaminating decoded fields.
    //   Bits [31:30] = mode        (preserved)
    //   Bits [29:27] = RESERVED    → mask out
    //   Bits [26:24] = TRD_NUM     (preserved)
    //   Bit  [23]    = RESERVED    → mask out
    //   Bits [22:16] = bank/dma/int/xip fields (preserved)
    //   Bits [15:0]  = CMD_TYPE    (preserved)
    constexpr uint32_t CMD_REG0_RESERVED_MASK = ~((0x7u << 27) | (0x1u << 23));
    uint32_t cmd_reg0_clean = cmd_reg0 & CMD_REG0_RESERVED_MASK;

    // Decode cmd_reg0 fields (Table 4.10) from the cleaned value
    uint8_t  thread_id  = static_cast<uint8_t>((cmd_reg0_clean >> 24) & 0x7u);  // [26:24] TRD_NUM
    uint8_t  bank_cs    = static_cast<uint8_t>((cmd_reg0_clean >> 20) & 0x7u);  // [22:20] BANK/CS
    bool     dma_sel    = ((cmd_reg0_clean >> 19) & 0x1u) != 0u;                 // [19]    DMA_SEL
    bool     int_flag   = ((cmd_reg0_clean >> 18) & 0x1u) != 0u;                 // [18]    INT
    bool     mb_xip_dis = ((cmd_reg0_clean >> 17) & 0x1u) != 0u;                 // [17]    MB_XIP_DIS
    bool     mb_xip_en  = ((cmd_reg0_clean >> 16) & 0x1u) != 0u;                 // [16]    MB_XIP_EN
    uint16_t cmd_type   = static_cast<uint16_t>(cmd_reg0_clean & 0xFFFFu);       // [15:0]  CMD_TYPE

    if (thread_id >= MAX_THREADS) {
        std::cerr << "[PIO_TRIGGER] ERROR: Invalid thread ID " << (int)thread_id << std::endl;
        return;
    }

    pio_thread_ctx_t& thread = pio_threads_[thread_id];
    if (thread.state != pio_thread_state_e::IDLE) {
        std::cerr << "[PIO_TRIGGER] WARNING: Thread " << (int)thread_id
                  << " not IDLE – ignoring" << std::endl;
        return;
    }

    // PIO-A FIX: snapshot only the registers required by this command type
    // (Section 4.4.2.1).  Unnecessary snapshots are suppressed to avoid
    // capturing stale values from a prior command.
    //
    //   RESET  (0x1100, 0x1101): cmd_reg0 only
    //   ERASE  (0x1001):         cmd_reg0 only
    //   ERASE  (0x1000):         cmd_reg0, cmd_reg1 (xSPI addr), cmd_reg4 (sector cnt), cmd_reg5
    //   PROGRAM/READ (0x21xx, 0x22xx): cmd_reg0..5 all required
    thread.cmd_reg1 = thread.cmd_reg2 = thread.cmd_reg3 =
    thread.cmd_reg4 = thread.cmd_reg5 = 0;  // clear; re-populate only what is needed

    switch (cmd_type) {
        case PIO_CMD_SECTOR_ERASE:
            // Requires xSPI address (cmd_reg1, cmd_reg5) and sector count (cmd_reg4)
            thread.cmd_reg1 = registers_[REG_CMD_REG1];
            thread.cmd_reg4 = registers_[REG_CMD_REG4];
            thread.cmd_reg5 = registers_[REG_CMD_REG5];
            break;
        case PIO_CMD_READ:
        case PIO_CMD_PROGRAM:
            // Requires all address and count registers
            thread.cmd_reg1 = registers_[REG_CMD_REG1];
            thread.cmd_reg2 = registers_[REG_CMD_REG2];
            thread.cmd_reg3 = registers_[REG_CMD_REG3];
            thread.cmd_reg4 = registers_[REG_CMD_REG4];
            thread.cmd_reg5 = registers_[REG_CMD_REG5];
            break;
        // PIO_CMD_CHIP_ERASE, PIO_CMD_RESET_SOFT, PIO_CMD_RESET_JEDEC:
        // only cmd_reg0 is required; no additional registers to snapshot.
        default:
            break;
    }

    thread.bank_cs    = bank_cs;
    thread.dma_sel    = dma_sel;
    thread.int_flag   = int_flag;
    thread.mb_xip_dis = mb_xip_dis;
    thread.mb_xip_en  = mb_xip_en;
    thread.cmd_type   = cmd_type;
    thread.status     = pio_status_reg_t();
    thread.state      = pio_thread_state_e::BUSY;

    // Mark thread busy in TRD_STATUS (1 bit per thread)
    registers_[REG_TRD_STATUS] |= (1u << thread_id);

    // DEFECT CDMA-9 / PIO side: set ctrl_busy when a thread becomes active
    set_ctrl_busy(true);

    std::cout << "[PIO_TRIGGER] Thread " << (int)thread_id
              << " CMD=0x" << std::hex << cmd_type << std::dec
              << " BANK=" << (int)bank_cs
              << " DMA_SEL=" << dma_sel
              << " INT=" << int_flag << std::endl;
}

// ── CDMA path  (Section 4.4.1) ────────────────────────────────────────────────

void cdns_xspi_acmd_tlm::cdma_handle_trigger(uint32_t cmd_reg0)
{
    // DEFECT CDMA-3 FIX: verify ctrl_config is set to ACMD work mode
    if ((registers_[REG_CTRL_CONFIG] & WORK_MODE_MASK) != WORK_MODE_ACMD) {
        std::cerr << "[CDMA_TRIGGER] ERROR: ctrl_config not in ACMD mode (0x"
                  << std::hex << registers_[REG_CTRL_CONFIG] << std::dec << ")\n";
        return;
    }

    // TRD_NUM is 3-bit field [26:24] per Table 4.3
    uint8_t thread_num = static_cast<uint8_t>((cmd_reg0 >> 24) & 0x7u);

    // CDMA-H FIX: Section 4.4.1 – if the target thread is already busy
    // (TRD_STATUS bit set), the command must be ignored and CMD_IGNORED must
    // be flagged in intr_status (0x110).  The controller does NOT start a
    // new operation on a busy thread.
    if (registers_[REG_TRD_STATUS] & (1u << thread_num)) {
        std::cerr << "[CDMA_TRIGGER] Thread " << (int)thread_num
                  << " is busy – command ignored (CDMA-H)\n";
        signal_cmd_ignored();   // CDMA-I: set CMD_IGNORED in intr_status
        return;
    }

    // Descriptor address from cmd_reg2 (LSB) + cmd_reg3 (MSB) per Tables 4.4/4.5
    uint32_t addr_lo   = registers_[REG_CMD_REG2];
    uint32_t addr_hi   = registers_[REG_CMD_REG3];
    uint64_t desc_addr = (static_cast<uint64_t>(addr_hi) << 32) | addr_lo;

    std::cout << "[CDMA_TRIGGER] Thread " << (int)thread_num
              << " descriptor=0x" << std::hex << desc_addr << std::dec << std::endl;

    // Alignment check (spec: address pointers must be 64-bit boundary aligned)
    if ((desc_addr & 0x3F) != 0) {
        std::cerr << "[CDMA_TRIGGER] ERROR: Descriptor address not 64-byte aligned\n";
        thread_status_[thread_num].cmd_error = true;
        thread_status_[thread_num].fail      = true;
        registers_[REG_TRD_ERR_INT] |= (1u << thread_num);
        update_interrupt();
        return;
    }

    // Mark thread busy before kicking the engine
    registers_[REG_TRD_STATUS] |= (1u << thread_num);

    // DEFECT CDMA-9 FIX: set ctrl_busy when a CDMA thread becomes active
    set_ctrl_busy(true);

    // Push thread into FETCHING state in the CDMA engine
    acmd_dma_thread_t& cdma_thread = acmd_dma_.threads_[thread_num];
    cdma_thread.descriptor_head_address = desc_addr;
    cdma_thread.state  = acmd_thread_state_e::FETCHING_DESCRIPTOR;
    cdma_thread.active = true;
}

// ── SystemC execution thread (polls every 10 ns) ─────────────────────────────

void cdns_xspi_acmd_tlm::command_execution_thread()
{
    std::cout << "[EXEC_THREAD] Started" << std::endl;
    wait(sc_core::sc_time(1, sc_core::SC_NS));

    while (true) {
        wait(sc_core::sc_time(10, sc_core::SC_NS));

        update_interrupt();

        // ── PIO processing ────────────────────────────────────────────────────
        for (auto& thread : pio_threads_) {
            if (thread.state == pio_thread_state_e::BUSY) {
                pio_execute_command(thread);
            }
        }

        // ── CDMA processing ───────────────────────────────────────────────────
        // DEFECT CDMA-7 FIX: advance all threads in one process_threads() call,
        // then iterate all threads to detect completion and sync registers.
        // Track which threads were active before the call.
        // CDMA-C: array sized to MAX_CDMA_THREADS (8) matching TRD_NUM width.
        bool prev_active[MAX_CDMA_THREADS];
        for (uint8_t i = 0; i < MAX_CDMA_THREADS; ++i)
            prev_active[i] = acmd_dma_.threads_[i].active;

        acmd_dma_.process_threads();   // advances ALL active threads by one state

        // Sync completion state back to shared register file for every thread
        // that just finished (was active, is now IDLE after COMPLETE or ERROR)
        for (uint8_t i = 0; i < MAX_CDMA_THREADS; ++i) {
            acmd_dma_thread_t& ct = acmd_dma_.threads_[i];
            bool just_finished = prev_active[i] && !ct.active;
            if (!just_finished) continue;

            reg_mutex_.lock();

            // Clear TRD_STATUS bit – thread is now IDLE
            registers_[REG_TRD_STATUS] &= ~(1u << i);

            if (ct.state == acmd_thread_state_e::IDLE) {
                // Thread completed the COMPLETE path; check if int_flag was set
                if (acmd_dma_.completion_interrupts_ & (1u << i)) {
                    registers_[REG_TRD_COMP_INT] |= (1u << i);
                    acmd_dma_.completion_interrupts_ &= ~static_cast<uint16_t>(1u << i);
                }
                // Also propagate error interrupts set inside process_threads()
                if (acmd_dma_.error_interrupts_ & (1u << i)) {
                    registers_[REG_TRD_ERR_INT] |= (1u << i);
                    acmd_dma_.error_interrupts_ &= ~static_cast<uint16_t>(1u << i);
                }
            }

            reg_mutex_.unlock();

            std::cout << "[CDMA_DONE] Thread " << (int)i
                      << ((registers_[REG_TRD_COMP_INT] & (1u << i)) ? " COMPLETE"
                                                                       : " ERROR/DONE")
                      << std::endl;
        }

        // DEFECT CDMA-9 FIX: clear ctrl_busy when no thread remains active
        if (!any_thread_active()) {
            set_ctrl_busy(false);
        }

        update_interrupt();
    }
}

// ── PIO command dispatch ──────────────────────────────────────────────────────

void cdns_xspi_acmd_tlm::pio_execute_command(pio_thread_ctx_t& thread)
{
    std::cout << "[PIO_EXEC] Thread " << (int)thread.thread_id
              << " CMD=0x" << std::hex << thread.cmd_type << std::dec << std::endl;

    bool success = false;
    switch (thread.cmd_type) {
        case PIO_CMD_READ:         success = pio_cmd_read(thread);         break;
        case PIO_CMD_PROGRAM:      success = pio_cmd_program(thread);      break;
        case PIO_CMD_SECTOR_ERASE: success = pio_cmd_sector_erase(thread); break;
        case PIO_CMD_CHIP_ERASE:   success = pio_cmd_chip_erase(thread);   break;
        case PIO_CMD_RESET_SOFT:
        case PIO_CMD_RESET_JEDEC:  success = pio_cmd_reset(thread);        break;
        default:
            std::cerr << "[PIO_EXEC] Unknown CMD=0x"
                      << std::hex << thread.cmd_type << std::dec << std::endl;
            thread.status.cmd_error = true;
            success = false;
    }

    reg_mutex_.lock();

    // Update thread status (COMPLETE always set, FAIL set on error – Table 4.17)
    thread.status.complete = true;
    if (!success) thread.status.fail = true;
    thread.state = success ? pio_thread_state_e::COMPLETE : pio_thread_state_e::FAIL;

    // Store in per-thread status array (retrieved via REG_CMD_STATUS_PTR / REG_CMD_STATUS)
    thread_status_[thread.thread_id] = thread.status;

    // Clear TRD_STATUS busy bit for this thread
    registers_[REG_TRD_STATUS] &= ~(1u << thread.thread_id);

    // Raise interrupt if requested (INT bit in cmd_reg0)
    if (thread.int_flag) {
        if (success) registers_[REG_TRD_COMP_INT] |= (1u << thread.thread_id);
        else         registers_[REG_TRD_ERR_INT]  |= (1u << thread.thread_id);
        update_interrupt();
    }

    // DEFECT CDMA-9 / PIO side: clear ctrl_busy if no more active threads
    if (!any_thread_active())
        set_ctrl_busy(false);

    thread.state = pio_thread_state_e::IDLE;

    reg_mutex_.unlock();

    std::cout << "[PIO_DONE] Thread " << (int)thread.thread_id
              << (success ? " COMPLETE" : " FAIL") << std::endl;
}

// ── PIO command implementations ───────────────────────────────────────────────

bool cdns_xspi_acmd_tlm::pio_cmd_read(pio_thread_ctx_t& t)
{
    // xSPI address: cmd_reg5 [63:32] | cmd_reg1 [31:0]  (Tables 4.11, 4.16)
    uint64_t xspi = (static_cast<uint64_t>(t.cmd_reg5) << 32) | t.cmd_reg1;
    // Host address: cmd_reg3 [63:32] | cmd_reg2 [31:0]  (Tables 4.12, 4.13)
    uint64_t mem  = (static_cast<uint64_t>(t.cmd_reg3) << 32) | t.cmd_reg2;
    // Byte count: cmd_reg4 = count – 1  (Table 4.15)
    uint32_t cnt  = t.cmd_reg4 + 1u;

    std::cout << "[PIO_READ] xSPI=0x" << std::hex << xspi
              << " mem=0x" << mem << std::dec << " bytes=" << cnt << std::endl;

    // DEFECT PIO-6 FIX: pass mb_xip_en to DMA transfer
    bool ok = pio_dma_transfer(xspi, mem, cnt, t.mb_xip_en);
    if (!ok) t.status.bus_error = true;
    return ok;
}

bool cdns_xspi_acmd_tlm::pio_cmd_program(pio_thread_ctx_t& t)
{
    uint64_t xspi = (static_cast<uint64_t>(t.cmd_reg5) << 32) | t.cmd_reg1;
    uint64_t mem  = (static_cast<uint64_t>(t.cmd_reg3) << 32) | t.cmd_reg2;
    uint32_t cnt  = t.cmd_reg4 + 1u;

    std::cout << "[PIO_PROG] mem=0x" << std::hex << mem
              << " xSPI=0x" << xspi << std::dec << " bytes=" << cnt << std::endl;

    // DEFECT PIO-6 FIX: pass mb_xip_en to DMA transfer
    bool ok = pio_dma_transfer(mem, xspi, cnt, t.mb_xip_en);
    if (!ok) t.status.bus_error = true;
    return ok;
}

bool cdns_xspi_acmd_tlm::pio_cmd_sector_erase(pio_thread_ctx_t& t)
{
    // xSPI address: cmd_reg5 | cmd_reg1  (Tables 4.11, 4.16)
    uint64_t addr = (static_cast<uint64_t>(t.cmd_reg5) << 32) | t.cmd_reg1;
    // Sector count: cmd_reg4 = SECT_CNT (value – 1)  (Table 4.14)
    uint32_t cnt  = t.cmd_reg4 + 1u;

    std::cout << "[PIO_ERASE] xSPI=0x" << std::hex << addr
              << std::dec << " sectors=" << cnt
              << " bank=" << (int)t.bank_cs << std::endl;
    return true;
}

bool cdns_xspi_acmd_tlm::pio_cmd_chip_erase(pio_thread_ctx_t& t)
{
    std::cout << "[PIO_CHIP_ERASE] bank=" << (int)t.bank_cs << std::endl;
    return true;
}

bool cdns_xspi_acmd_tlm::pio_cmd_reset(pio_thread_ctx_t& t)
{
    bool jedec = (t.cmd_type == PIO_CMD_RESET_JEDEC);
    std::cout << "[PIO_RESET] " << (jedec ? "JEDEC (0x1101)" : "Soft (0x1100)")
              << " bank=" << (int)t.bank_cs << std::endl;
    return true;
}

// DEFECT PIO-6 FIX: mb_xip_en parameter restored
bool cdns_xspi_acmd_tlm::pio_dma_transfer(uint64_t src, uint64_t dst,
                                           uint32_t size, bool mb_xip_en)
{
    // When XIP mode entry is requested, the controller sends mode bits to the
    // device before the read.  In this functional model we acknowledge the flag
    // but otherwise complete the transfer normally.
    if (mb_xip_en) {
        std::cout << "[PIO_DMA] MB_XIP_EN asserted – XIP mode entry before transfer\n";
    }

    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    auto* buf = new unsigned char[size];

    // Step 1: read from source
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(src);
    trans.set_data_ptr(buf);
    trans.set_data_length(size);
    trans.set_streaming_width(size);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    axi_master_socket->b_transport(trans, delay);

    if (trans.is_response_error()) {
        std::cerr << "[PIO_DMA] Read failed from 0x" << std::hex << src << std::dec << std::endl;
        delete[] buf;
        return false;
    }

    // Step 2: write to destination
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(dst);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    delay = sc_core::SC_ZERO_TIME;
    axi_master_socket->b_transport(trans, delay);

    bool ok = !trans.is_response_error();
    if (!ok)
        std::cerr << "[PIO_DMA] Write failed to 0x" << std::hex << dst << std::dec << std::endl;

    delete[] buf;
    return ok;
}

// ── Interrupt update (single driver: only called from execution thread) ───────

void cdns_xspi_acmd_tlm::update_interrupt()
{
    bool active = (registers_[REG_TRD_COMP_INT] != 0)
               || (registers_[REG_TRD_ERR_INT]  != 0);

    bool prev = interrupt_out.read();
    interrupt_out.write(active);

    // Only log on state transition to avoid flooding output every 10-ns poll cycle.
    if (active && !prev) {
        std::cout << "[INTERRUPT] Line asserted  (COMP=0x"
                  << std::hex << registers_[REG_TRD_COMP_INT]
                  << " ERR=0x" << registers_[REG_TRD_ERR_INT]
                  << std::dec << ")" << std::endl;
    } else if (!active && prev) {
        std::cout << "[INTERRUPT] Line de-asserted" << std::endl;
    }
}

} // namespace mylibrary