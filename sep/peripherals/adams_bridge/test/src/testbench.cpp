// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file testbench.cpp
 * @brief Harness implementation and sc_main for the Adams Bridge tests.
 */

#include "abr_testbench.h"

#include "csml_logger.h"
#include "csml_parameter.h"

#include <tlm_utils/tlm_quantumkeeper.h>

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>

using sc_core::SC_NS;
using sc_core::SC_US;
using sc_core::sc_time;

namespace {

/// Poll budget for a status bit. Each iteration advances 1 us of simulated
/// time, so this is a generous multiple of the slowest modeled operation
/// (ML-DSA sign, 78000 cycles at 100 MHz = 780 us).
constexpr unsigned int POLL_ITERATIONS = 5000u;
constexpr double POLL_STEP_US = 1.0;

} // namespace

abr_testbench::abr_testbench(sc_core::sc_module_name n)
    : sc_core::sc_module(n),
      isock("isock"),
      kv_isock("kv_isock"),
      km_mldsa_seed_isock("km_mldsa_seed_isock"),
      km_mlkem_d_isock("km_mlkem_d_isock"),
      km_mlkem_z_isock("km_mlkem_z_isock"),
      km_mlkem_msg_isock("km_mlkem_msg_isock"),
      clk_sig("clk_sig"),
      rst_sig("rst_sig"),
      err_sig("err_sig"),
      notif_sig("notif_sig"),
      dut("abr")
{
    isock.bind(dut.target_socket);
    kv_isock.bind(dut.keymgr_tl_socket);
    km_mldsa_seed_isock.bind(dut.keymgr_mldsa_seed_socket);
    km_mlkem_d_isock.bind(dut.keymgr_mlkem_d_socket);
    km_mlkem_z_isock.bind(dut.keymgr_mlkem_z_socket);
    km_mlkem_msg_isock.bind(dut.keymgr_mlkem_msg_socket);

    dut.clk_i(clk_sig);
    dut.rst_ni(rst_sig);
    dut.intr_abr_error(err_sig);
    dut.intr_abr_notif(notif_sig);

    clk_sig.write(100000000.0); // 100 MHz
    rst_sig.write(true);

    SC_THREAD(main_thread);
}

// =============================================================================
// Bus access
// =============================================================================

void abr_testbench::transact(tlm_utils::simple_initiator_socket<abr_testbench, 32> &sock,
                             tlm::tlm_command cmd, uint64_t addr, unsigned char *data,
                             unsigned int len)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = sc_core::SC_ZERO_TIME;

    trans.set_command(cmd);
    trans.set_address(addr);
    trans.set_data_ptr(data);
    trans.set_data_length(len);
    trans.set_streaming_width(len);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    sock->b_transport(trans, delay);
}

uint32_t abr_testbench::rd(uint32_t offset)
{
    // Write-only offsets leave the buffer untouched (CSML reports the access
    // and returns no data), so pre-zero it: that is what software observes.
    uint32_t value = 0u;
    transact(isock, tlm::TLM_READ_COMMAND, offset,
             reinterpret_cast<unsigned char *>(&value), sizeof(value));
    return value;
}

void abr_testbench::wr(uint32_t offset, uint32_t value)
{
    transact(isock, tlm::TLM_WRITE_COMMAND, offset,
             reinterpret_cast<unsigned char *>(&value), sizeof(value));
}

std::vector<uint32_t> abr_testbench::rd_n(uint32_t offset, unsigned int count)
{
    std::vector<uint32_t> out(count, 0u);
    for (unsigned int i = 0; i < count; ++i) {
        out[i] = rd(offset + i * 4u);
    }
    return out;
}

void abr_testbench::wr_n(uint32_t offset, const std::vector<uint32_t> &values)
{
    for (std::size_t i = 0; i < values.size(); ++i) {
        wr(offset + static_cast<uint32_t>(i) * 4u, values[i]);
    }
}

void abr_testbench::kv_push(unsigned int entry, const std::vector<uint8_t> &material)
{
    std::vector<uint8_t> buf(material);
    if (buf.empty()) {
        return;
    }
    transact(kv_isock, tlm::TLM_WRITE_COMMAND,
             static_cast<uint64_t>(entry) * abr::KV_ENTRY_BYTES, buf.data(),
             static_cast<unsigned int>(buf.size()));
}

std::vector<uint8_t> abr_testbench::kv_read(unsigned int entry, unsigned int len)
{
    std::vector<uint8_t> buf(len, 0u);
    transact(kv_isock, tlm::TLM_READ_COMMAND,
             static_cast<uint64_t>(entry) * abr::KV_ENTRY_BYTES, buf.data(), len);
    return buf;
}

bool abr_testbench::kv_access_ok(uint64_t addr, unsigned int len)
{
    std::vector<uint8_t> buf(len, 0u);

    tlm::tlm_generic_payload trans;
    sc_time delay = sc_core::SC_ZERO_TIME;

    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(buf.data());
    trans.set_data_length(len);
    trans.set_streaming_width(len);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    kv_isock->b_transport(trans, delay);
    return trans.get_response_status() == tlm::TLM_OK_RESPONSE;
}

tlm_utils::simple_initiator_socket<abr_testbench, 32> &
abr_testbench::km_lane_sock(unsigned int lane)
{
    switch (lane) {
    case 1u:
        return km_mlkem_d_isock;
    case 2u:
        return km_mlkem_z_isock;
    case 3u:
        return km_mlkem_msg_isock;
    default:
        return km_mldsa_seed_isock;
    }
}

void abr_testbench::km_share_commit(unsigned int lane, const std::vector<uint32_t> &words,
                                    bool valid)
{
    auto &sock = km_lane_sock(lane);
    const unsigned int n = std::min<unsigned int>(
        static_cast<unsigned int>(words.size()), abr::KM_SHARE_WORDS);
    for (unsigned int w = 0; w < n; ++w) {
        const uint32_t mask = 0xA5A5A5A5u ^ (w * 0x01010101u);
        uint32_t share0 = mask;
        uint32_t share1 = words[w] ^ mask;
        transact(sock, tlm::TLM_WRITE_COMMAND, abr::KM_SHARE0_BASE + w * 4u,
                 reinterpret_cast<unsigned char *>(&share0), 4u);
        transact(sock, tlm::TLM_WRITE_COMMAND, abr::KM_SHARE1_BASE + w * 4u,
                 reinterpret_cast<unsigned char *>(&share1), 4u);
    }
    uint32_t ctrl = valid ? 1u : 0u;
    transact(sock, tlm::TLM_WRITE_COMMAND, abr::KM_KEY_CTRL_OFF,
             reinterpret_cast<unsigned char *>(&ctrl), 4u);
}

bool abr_testbench::km_share_access_ok(unsigned int lane, uint64_t addr, unsigned int len,
                                       bool is_write)
{
    std::vector<uint8_t> buf(len == 0u ? 1u : len, 0u);

    tlm::tlm_generic_payload trans;
    sc_time delay = sc_core::SC_ZERO_TIME;

    trans.set_command(is_write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(buf.data());
    trans.set_data_length(len);
    trans.set_streaming_width(len == 0u ? 1u : len);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    km_lane_sock(lane)->b_transport(trans, delay);
    return trans.get_response_status() == tlm::TLM_OK_RESPONSE;
}

// =============================================================================
// Sequencing
// =============================================================================

void abr_testbench::reset_dut()
{
    rst_sig.write(false);
    wait(sc_time(1, SC_US));
    rst_sig.write(true);
    wait(sc_time(1, SC_US));
}

bool abr_testbench::poll_status(uint32_t offset, uint32_t mask, bool expect_set)
{
    for (unsigned int i = 0; i < POLL_ITERATIONS; ++i) {
        const bool is_set = (rd(offset) & mask) != 0u;
        if (is_set == expect_set) {
            return true;
        }
        wait(sc_time(POLL_STEP_US, SC_US));
    }
    return false;
}

bool abr_testbench::wait_mldsa_ready()
{
    return poll_status(abr::OFF_MLDSA_STATUS, 0x1u, true);
}

bool abr_testbench::wait_mldsa_valid()
{
    return poll_status(abr::OFF_MLDSA_STATUS, 0x2u, true);
}

bool abr_testbench::run_mldsa(uint32_t ctrl_value)
{
    if (!wait_mldsa_ready()) {
        return false;
    }
    wr(abr::OFF_MLDSA_CTRL, ctrl_value);
    return wait_mldsa_valid();
}

bool abr_testbench::zeroize_mldsa()
{
    wr(abr::OFF_MLDSA_CTRL, 1u << 3);
    return wait_mldsa_ready();
}

bool abr_testbench::wait_mlkem_ready()
{
    return poll_status(abr::OFF_MLKEM_STATUS, 0x1u, true);
}

bool abr_testbench::wait_mlkem_valid()
{
    return poll_status(abr::OFF_MLKEM_STATUS, 0x2u, true);
}

bool abr_testbench::run_mlkem(uint32_t ctrl_value)
{
    if (!wait_mlkem_ready()) {
        return false;
    }
    wr(abr::OFF_MLKEM_CTRL, ctrl_value);
    return wait_mlkem_valid();
}

bool abr_testbench::zeroize_mlkem()
{
    wr(abr::OFF_MLKEM_CTRL, 1u << 3);
    return wait_mlkem_ready();
}

// =============================================================================
// Result tracking
// =============================================================================

void abr_testbench::section(const std::string &title)
{
    std::cout << "\n--- " << title << " ---" << std::endl;
}

void abr_testbench::check(bool condition, const std::string &what)
{
    ++m_tests_run;
    if (condition) {
        std::cout << "  [PASS] " << what << std::endl;
    } else {
        ++m_tests_failed;
        std::cout << "  [FAIL] " << what << std::endl;
    }
}

void abr_testbench::check_eq(uint32_t got, uint32_t expected, const std::string &what)
{
    ++m_tests_run;
    if (got == expected) {
        std::cout << "  [PASS] " << what << std::endl;
    } else {
        ++m_tests_failed;
        std::cout << "  [FAIL] " << what << " (got 0x" << std::hex << std::setw(8)
                  << std::setfill('0') << got << ", expected 0x" << std::setw(8)
                  << expected << std::dec << std::setfill(' ') << ")" << std::endl;
    }
}

// =============================================================================
// Main sequence
// =============================================================================

void abr_testbench::main_thread()
{
    reset_dut();

    register_tests(*this);
    mldsa_tests(*this);
    mlkem_tests(*this);
    nist_kat_tests(*this);
    interrupt_kv_tests(*this);
    coverage_tests(*this);

    std::cout << "\n==========================================" << std::endl;
    std::cout << "Adams Bridge testbench summary" << std::endl;
    std::cout << "  checks run    : " << m_tests_run << std::endl;
    std::cout << "  checks failed : " << m_tests_failed << std::endl;
    std::cout << "  result        : " << (m_tests_failed == 0 ? "PASS" : "FAIL")
              << std::endl;
    std::cout << "==========================================" << std::endl;

    sc_core::sc_stop();
}

#ifdef __COVERAGE__
extern "C" void __gcov_dump(void);
#endif

int sc_main(int argc, char *argv[])
{
    CsmlLogger logger;
    logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    // Registers the CCI broker; csml_param construction depends on it.
    load_config_file(argc > 1 ? argv[1] : nullptr);

    // Temporal decoupling: a 1 us quantum is short relative to the modeled
    // operation latencies, so the engine threads resynchronize several times
    // per command and the LT run-ahead stays bounded.
    tlm::tlm_global_quantum::instance().set(sc_time(1, SC_US));

    std::cout << "Starting Adams Bridge SystemC TLM testbench..." << std::endl;

    abr_testbench tb("abr_testbench");

    sc_core::sc_start();

    const int rc = (tb.m_tests_failed > 0) ? 1 : 0;

#ifdef __COVERAGE__
    __gcov_dump();
#endif
    std::quick_exit(rc);

    return rc;
}
