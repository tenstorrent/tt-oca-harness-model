// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file abr_testbench.h
 * @brief Testbench harness for the Adams Bridge model.
 *
 * Provides a TLM initiator into the ABR register aperture, a second initiator
 * into the Key Vault sideload port, reset control, and the poll/command
 * sequencing helpers that mirror what SEP firmware does (see
 * sw/sep-vp-tests/.../common/abr_mldsa.h).
 */

#pragma once

#include "adams_bridge.h"

#include <systemc>
#include <tlm_utils/simple_initiator_socket.h>

#include <cstdint>
#include <string>
#include <vector>

class abr_testbench : public sc_core::sc_module
{
  public:
    SC_HAS_PROCESS(abr_testbench);
    explicit abr_testbench(sc_core::sc_module_name n);

    // --- Bus access -----------------------------------------------------------

    /// Read a 32-bit register. Write-only offsets read back as 0.
    uint32_t rd(uint32_t offset);

    /// Write a 32-bit register.
    void wr(uint32_t offset, uint32_t value);

    /// Read @p count consecutive words starting at @p offset.
    std::vector<uint32_t> rd_n(uint32_t offset, unsigned int count);

    /// Write a word array starting at @p offset.
    void wr_n(uint32_t offset, const std::vector<uint32_t> &values);

    /// Push key material into a Key Vault entry over the sideload port.
    void kv_push(unsigned int entry, const std::vector<uint8_t> &material);

    /// Read key material back out of a Key Vault entry over the sideload port.
    std::vector<uint8_t> kv_read(unsigned int entry, unsigned int len);

    /// Raw sideload access; returns true when the model accepted the address.
    bool kv_access_ok(uint64_t addr, unsigned int len);

    /// HMAC-style dual-share commit on a KM destination socket (lane 0..3).
    void km_share_commit(unsigned int lane, const std::vector<uint32_t> &words,
                         bool valid = true);

    /// Raw access on a KM share socket; returns true when accepted.
    bool km_share_access_ok(unsigned int lane, uint64_t addr, unsigned int len,
                            bool is_write);

    // --- Sequencing -----------------------------------------------------------

    /// Pulse rst_ni low then high, leaving the model in its reset state.
    void reset_dut();

    /// Poll MLDSA_STATUS until READY is set.
    bool wait_mldsa_ready();

    /// Poll MLDSA_STATUS until VALID is set.
    bool wait_mldsa_valid();

    /// Full firmware-style command: wait READY, write CTRL, wait VALID.
    bool run_mldsa(uint32_t ctrl_value);

    /// Write CTRL.ZEROIZE and wait for READY to return.
    bool zeroize_mldsa();

    bool wait_mlkem_ready();
    bool wait_mlkem_valid();
    bool run_mlkem(uint32_t ctrl_value);
    bool zeroize_mlkem();

    // --- Result tracking -------------------------------------------------------

    void check(bool condition, const std::string &what);
    void check_eq(uint32_t got, uint32_t expected, const std::string &what);
    void section(const std::string &title);

    unsigned int m_tests_run = 0;
    unsigned int m_tests_failed = 0;

    // --- Structure ------------------------------------------------------------

    tlm_utils::simple_initiator_socket<abr_testbench, 32> isock;
    tlm_utils::simple_initiator_socket<abr_testbench, 32> kv_isock;
    tlm_utils::simple_initiator_socket<abr_testbench, 32> km_mldsa_seed_isock;
    tlm_utils::simple_initiator_socket<abr_testbench, 32> km_mlkem_d_isock;
    tlm_utils::simple_initiator_socket<abr_testbench, 32> km_mlkem_z_isock;
    tlm_utils::simple_initiator_socket<abr_testbench, 32> km_mlkem_msg_isock;

    sc_core::sc_signal<double> clk_sig;
    sc_core::sc_signal<bool> rst_sig;
    sc_core::sc_signal<bool> err_sig;
    sc_core::sc_signal<bool> notif_sig;

    abr_ip dut;

  private:
    void main_thread();

    /// Shared b_transport driver for initiator sockets.
    void transact(tlm_utils::simple_initiator_socket<abr_testbench, 32> &sock,
                  tlm::tlm_command cmd, uint64_t addr, unsigned char *data,
                  unsigned int len);

    tlm_utils::simple_initiator_socket<abr_testbench, 32> &km_lane_sock(unsigned int lane);

    /// Poll a status register for a bit, bounded so a hang fails rather than spins.
    bool poll_status(uint32_t offset, uint32_t mask, bool expect_set);
};

// Test groups, each in its own translation unit.
void register_tests(abr_testbench &tb);
void mldsa_tests(abr_testbench &tb);
void mlkem_tests(abr_testbench &tb);
void interrupt_kv_tests(abr_testbench &tb);
void nist_kat_tests(abr_testbench &tb);
