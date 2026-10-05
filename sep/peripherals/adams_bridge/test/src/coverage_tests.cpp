// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file coverage_tests.cpp
 * @brief Edge paths not exercised by the functional / KAT suites.
 *
 * These cases exist to keep line coverage on the touched model sources
 * (src/adams_bridge.cpp, abr_crypto.cpp, abr_pqc_*.c) at the repository
 * ≥ 95% gate. They are still behavioural checks, not no-ops.
 */

#include "abr_testbench.h"
#include "abr_pqc_wrap.h"

#include <cci_configuration>

#include <memory>

namespace {

constexpr uint32_t CMD_KEYGEN        = 0x1u;
constexpr uint32_t CMD_SIGN          = 0x2u;
constexpr uint32_t CMD_VERIFY        = 0x3u;
constexpr uint32_t CMD_KEYGEN_SIGN   = 0x4u;
constexpr uint32_t CMD_ENCAPS        = 0x2u;
constexpr uint32_t CMD_KEYGEN_DECAPS = 0x4u;

constexpr uint32_t CTRL_STREAM_MSG = 1u << 6;

constexpr uint32_t KV_READ_EN     = 1u << 0;
constexpr uint32_t KV_ENTRY_SHIFT = 1u;
constexpr uint32_t KV_STATUS_VALID = 1u << 1;
constexpr uint32_t KV_ERROR_SHIFT  = 2u;
constexpr uint32_t KV_ERROR_MASK   = 0xFFu;

constexpr uint32_t ST_READY = 1u << 0;

/// Fast stand-in so coverage cases do not pay FIPS keygen cost.
class stub_backend : public abr::abr_crypto_backend
{
  public:
    const char *name() const override { return "coverage-stub"; }
    bool is_standards_conformant() const override { return false; }

    void mldsa_keygen(const abr::bytes &, abr::bytes &pubkey, abr::bytes &privkey) override
    {
        pubkey.assign(abr::MLDSA_PUBKEY_BYTES, 0xABu);
        privkey.assign(abr::MLDSA_PRIVKEY_BYTES, 0xCDu);
    }
    void mldsa_sign(const abr::bytes &, const abr::bytes &, const abr::bytes &,
                    abr::bytes &signature) override
    {
        signature.assign(abr::MLDSA_SIG_BYTES, 0xEFu);
    }
    void mldsa_verify(const abr::bytes &, const abr::bytes &, const abr::bytes &,
                      abr::bytes &ctilde_out) override
    {
        ctilde_out.assign(abr::MLDSA_CTILDE_BYTES, 0x11u);
    }
    void mldsa_compute_mu(const abr::bytes &, const abr::bytes &, const abr::bytes &,
                          abr::bytes &mu_out) override
    {
        mu_out.assign(abr::MLDSA_MU_BYTES, 0x22u);
    }
    void mldsa_compute_mu_from_sk(const abr::bytes &, const abr::bytes &,
                                  const abr::bytes &, abr::bytes &mu_out) override
    {
        mu_out.assign(abr::MLDSA_MU_BYTES, 0x22u);
    }
    void mlkem_keygen(const abr::bytes &, const abr::bytes &, abr::bytes &encaps_key,
                      abr::bytes &decaps_key) override
    {
        encaps_key.assign(abr::MLKEM_ENCAPS_KEY_BYTES, 0x33u);
        decaps_key.assign(abr::MLKEM_DECAPS_KEY_BYTES, 0x44u);
    }
    void mlkem_encaps(const abr::bytes &, const abr::bytes &, abr::bytes &ciphertext,
                      abr::bytes &shared_key) override
    {
        ciphertext.assign(abr::MLKEM_CIPHERTEXT_BYTES, 0x55u);
        shared_key.assign(abr::MLKEM_SHARED_KEY_BYTES, 0x66u);
    }
    void mlkem_decaps(const abr::bytes &, const abr::bytes &,
                      abr::bytes &shared_key) override
    {
        shared_key.assign(abr::MLKEM_SHARED_KEY_BYTES, 0x77u);
    }
};

/// Undersized outputs force bytes_to_regs() through the short-buffer lane.
class short_backend : public abr::abr_crypto_backend
{
  public:
    const char *name() const override { return "short-output"; }
    bool is_standards_conformant() const override { return false; }

    void mldsa_keygen(const abr::bytes &, abr::bytes &pubkey, abr::bytes &privkey) override
    {
        pubkey.assign(3u, 0x11u);
        privkey.assign(3u, 0x22u);
    }
    void mldsa_sign(const abr::bytes &, const abr::bytes &, const abr::bytes &,
                    abr::bytes &signature) override
    {
        signature.assign(2u, 0x33u);
    }
    void mldsa_verify(const abr::bytes &, const abr::bytes &, const abr::bytes &,
                      abr::bytes &ctilde_out) override
    {
        ctilde_out.assign(1u, 0x44u);
    }
    void mldsa_compute_mu(const abr::bytes &, const abr::bytes &, const abr::bytes &,
                          abr::bytes &mu_out) override
    {
        mu_out.assign(1u, 0x55u);
    }
    void mldsa_compute_mu_from_sk(const abr::bytes &, const abr::bytes &,
                                  const abr::bytes &, abr::bytes &mu_out) override
    {
        mu_out.assign(1u, 0x55u);
    }
    void mlkem_keygen(const abr::bytes &, const abr::bytes &, abr::bytes &encaps_key,
                      abr::bytes &decaps_key) override
    {
        encaps_key.assign(2u, 0x66u);
        decaps_key.assign(2u, 0x77u);
    }
    void mlkem_encaps(const abr::bytes &, const abr::bytes &, abr::bytes &ciphertext,
                      abr::bytes &shared_key) override
    {
        ciphertext.assign(2u, 0x88u);
        shared_key.assign(2u, 0x99u);
    }
    void mlkem_decaps(const abr::bytes &, const abr::bytes &,
                      abr::bytes &shared_key) override
    {
        shared_key.assign(2u, 0xAAu);
    }
};

tlm::tlm_response_status raw_xact(tlm_utils::simple_initiator_socket<abr_testbench, 32> &sock,
                                  tlm::tlm_command cmd, uint64_t addr, unsigned char *data,
                                  unsigned int len, unsigned int streaming_width = 0u,
                                  unsigned char *be = nullptr, unsigned int be_len = 0u)
{
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    trans.set_command(cmd);
    trans.set_address(addr);
    trans.set_data_ptr(data);
    trans.set_data_length(len);
    const unsigned int sw =
        streaming_width != 0u ? streaming_width : (len == 0u ? 1u : len);
    trans.set_streaming_width(sw);
    trans.set_byte_enable_ptr(be);
    trans.set_byte_enable_length(be_len);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    sock->b_transport(trans, delay);
    return trans.get_response_status();
}

unsigned int raw_dbg(tlm_utils::simple_initiator_socket<abr_testbench, 32> &sock,
                     tlm::tlm_command cmd, uint64_t addr, unsigned char *data,
                     unsigned int len)
{
    tlm::tlm_generic_payload trans;
    trans.set_command(cmd);
    trans.set_address(addr);
    trans.set_data_ptr(data);
    trans.set_data_length(len);
    trans.set_streaming_width(len == 0u ? 1u : len);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    return sock->transport_dbg(trans);
}

bool raw_dmi(tlm_utils::simple_initiator_socket<abr_testbench, 32> &sock, uint64_t addr)
{
    tlm::tlm_generic_payload trans;
    tlm::tlm_dmi dmi;
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(nullptr);
    trans.set_data_length(0);
    return sock->get_direct_mem_ptr(trans, dmi);
}

void set_int_param(regmodel::Param<int> &p, int value)
{
    auto broker = cci::cci_get_broker();
    cci::cci_param_handle h = broker.get_param_handle(p.get_Name());
    if (h.is_valid()) {
        h.set_cci_value(cci::cci_value(value));
    }
}

} // namespace

void coverage_tests(abr_testbench &tb)
{
    tb.section("Coverage: clock fallback and zero-cycle charge");

    tb.reset_dut();
    tb.dut.set_crypto_backend(std::make_unique<stub_backend>());

    tb.clk_sig.write(0.0);
    tb.check(tb.zeroize_mldsa(), "zeroize with clk_i=0 uses default_clk_freq_hz");
    tb.clk_sig.write(100000000.0);

    // Exercises the sub-quantum resync branch in finish_mldsa()/finish_mlkem().
    // 10 cycles at 100 MHz is 100 ns, well under the 1 us quantum, so
    // consume_cycles() only accumulates local time instead of syncing, and the
    // `m_qk.get_local_time() > SC_ZERO_TIME` check in finish_* is what makes
    // the result visible.
    //
    // The 200 ns waits below are load-bearing: long enough for the 100 ns
    // operation to complete, short enough to stay inside the 1 us quantum.
    // Raising them past 1 us makes the quantum keeper sync on its own and
    // silently stops testing this branch; lowering them below 100 ns samples
    // STATUS before the operation finishes.
    const int saved_mldsa_kg = tb.dut.mldsa_keygen_cycles.get_param_value();
    const int saved_mlkem_kg = tb.dut.mlkem_keygen_cycles.get_param_value();
    const int saved_zeroize_early = tb.dut.zeroize_cycles.get_param_value();
    set_int_param(tb.dut.mldsa_keygen_cycles, 10);
    set_int_param(tb.dut.mlkem_keygen_cycles, 10);
    set_int_param(tb.dut.zeroize_cycles, 1);
    tb.wr(abr::OFF_MLDSA_CTRL, 1u << 3);
    sc_core::wait(sc_core::sc_time(200, sc_core::SC_NS));
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & ST_READY, ST_READY,
                "short zeroize restores ML-DSA READY inside the quantum");
    tb.wr(abr::OFF_MLDSA_CTRL, CMD_KEYGEN);
    sc_core::wait(sc_core::sc_time(200, sc_core::SC_NS));
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & 0x2u, 0x2u,
                "sub-quantum ML-DSA keygen sets VALID");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_PUBKEY), 0xABABABABu,
                "sub-quantum ML-DSA keygen publishes the stub public key");
    tb.wr(abr::OFF_MLKEM_CTRL, 1u << 3);
    sc_core::wait(sc_core::sc_time(200, sc_core::SC_NS));
    tb.wr(abr::OFF_MLKEM_CTRL, CMD_KEYGEN);
    sc_core::wait(sc_core::sc_time(200, sc_core::SC_NS));
    tb.check_eq(tb.rd(abr::OFF_MLKEM_STATUS) & 0x2u, 0x2u,
                "sub-quantum ML-KEM keygen sets VALID");
    tb.check_eq(tb.rd(abr::OFF_MLKEM_ENCAPS_KEY), 0x33333333u,
                "sub-quantum ML-KEM keygen publishes the stub encaps key");
    set_int_param(tb.dut.zeroize_cycles, saved_zeroize_early);
    tb.check(tb.zeroize_mldsa() && tb.zeroize_mlkem(),
             "zeroize both engines after the sub-quantum keygens");
    set_int_param(tb.dut.mldsa_keygen_cycles, saved_mldsa_kg);
    set_int_param(tb.dut.mlkem_keygen_cycles, saved_mlkem_kg);

    const int saved_kv_cycles = tb.dut.kv_access_cycles.get_param_value();
    set_int_param(tb.dut.kv_access_cycles, 0);
    tb.check(tb.kv_access_ok(0u, 4u), "KV access with a 0-cycle latency still succeeds");
    set_int_param(tb.dut.kv_access_cycles, saved_kv_cycles);

    const int saved_zeroize = tb.dut.zeroize_cycles.get_param_value();
    set_int_param(tb.dut.zeroize_cycles, 0);
    tb.check(tb.zeroize_mlkem(), "zeroize with 0 modeled cycles is a no-op wait");
    set_int_param(tb.dut.zeroize_cycles, saved_zeroize);

    tb.section("Coverage: engine ignores commands issued under reset");

    tb.rst_sig.write(false);
    sc_core::wait(sc_core::sc_time(1, sc_core::SC_US));
    tb.wr(abr::OFF_MLDSA_CTRL, CMD_KEYGEN);
    tb.wr(abr::OFF_MLKEM_CTRL, CMD_KEYGEN);
    sc_core::wait(sc_core::sc_time(1, sc_core::SC_US));
    // Release first so reset_dut() can produce a fresh falling edge.
    tb.rst_sig.write(true);
    sc_core::wait(sc_core::sc_time(1, sc_core::SC_US));
    tb.reset_dut();
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & ST_READY, ST_READY,
                "reset after an in-reset KEYGEN restores READY");
    tb.check_eq(tb.rd(abr::OFF_MLKEM_STATUS) & ST_READY, ST_READY,
                "reset after an in-reset ML-KEM KEYGEN restores READY");

    tb.section("Coverage: concurrent ML-DSA and ML-KEM (stub)");

    // Stub backend keeps this cheap: start both engines before either completes.
    tb.check(tb.zeroize_mldsa() && tb.zeroize_mlkem(), "zeroize both engines before overlap");
    tb.wr(abr::OFF_MLDSA_CTRL, CMD_KEYGEN);
    tb.wr(abr::OFF_MLKEM_CTRL, CMD_KEYGEN);
    const bool mldsa_done = tb.wait_mldsa_valid();
    const bool mlkem_done = tb.wait_mlkem_valid();
    tb.check(mldsa_done && mlkem_done,
             "overlapping ML-DSA and ML-KEM KEYGENs both reach VALID");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_PUBKEY), 0xABABABABu,
                "overlapping ML-DSA KEYGEN published the stub public key");
    tb.check_eq(tb.rd(abr::OFF_MLKEM_ENCAPS_KEY), 0x33333333u,
                "overlapping ML-KEM KEYGEN published the stub encapsulation key");

    tb.section("Coverage: register-aperture TLM matrix");

    // Use MLDSA_MSG[0] as a known RW-ish window (WO decode still stores via
    // callback) — snapshot STATUS which must not change on rejected ops.
    const uint32_t status_snap = tb.rd(abr::OFF_MLDSA_STATUS);
    const uint32_t name0_snap = tb.rd(abr::OFF_MLDSA_NAME);
    uint32_t poke = 0xA5A5A5A5u;

    tb.check(raw_xact(tb.isock, tlm::TLM_IGNORE_COMMAND, abr::OFF_MLDSA_NAME,
                      reinterpret_cast<unsigned char *>(&poke), 4u) ==
                 tlm::TLM_COMMAND_ERROR_RESPONSE,
             "register IGNORE returns COMMAND_ERROR");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_NAME), name0_snap,
                "register IGNORE leaves NAME unchanged");
    tb.check_eq(poke, 0xA5A5A5A5u, "register IGNORE does not mutate the initiator buffer");

    tb.check(raw_xact(tb.isock, tlm::TLM_WRITE_COMMAND, abr::OFF_MLDSA_MSG, nullptr, 4u) ==
                 tlm::TLM_GENERIC_ERROR_RESPONSE,
             "register null data_ptr returns GENERIC_ERROR");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS), status_snap,
                "null-pointer register write leaves STATUS unchanged");

    uint32_t zlen = 0u;
    tb.check(raw_xact(tb.isock, tlm::TLM_WRITE_COMMAND, abr::OFF_MLDSA_MSG,
                      reinterpret_cast<unsigned char *>(&zlen), 0u) ==
                 tlm::TLM_BURST_ERROR_RESPONSE,
             "register zero-length write returns BURST_ERROR");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS), status_snap,
                "zero-length register write leaves STATUS unchanged");

    // Beyond the 64 KiB aperture: reserved / RAZ — must not touch NAME.
    uint32_t oob = 0xDEADBEEFu;
    tb.check(raw_xact(tb.isock, tlm::TLM_WRITE_COMMAND, abr::ABR_APERTURE_SIZE,
                      reinterpret_cast<unsigned char *>(&oob), 4u) ==
                 tlm::TLM_OK_RESPONSE,
             "register write past the aperture is accepted as reserved");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_NAME), name0_snap,
                "OOB register write does not mutate NAME");

    // Unaligned 2-byte read of NAME: Memory returns the mid-word lanes.
    uint16_t half = 0u;
    tb.check(raw_xact(tb.isock, tlm::TLM_READ_COMMAND, abr::OFF_MLDSA_NAME + 1u,
                      reinterpret_cast<unsigned char *>(&half), 2u) ==
                 tlm::TLM_OK_RESPONSE,
             "unaligned 2-byte register read is accepted");
    const uint16_t name_mid =
        static_cast<uint16_t>((name0_snap >> 8) & 0xFFFFu);
    tb.check_eq(static_cast<uint32_t>(half), static_cast<uint32_t>(name_mid),
                "unaligned read returns the middle half-word of NAME");

    // Illegal byte-enable length 0 with a non-null pointer.
    unsigned char be0[1] = {0xFFu};
    uint32_t be_data = 0xFFFFFFFFu;
    tb.wr(abr::OFF_MLDSA_SIGNATURE, 0x11223344u);
    const uint32_t sig_before_be = tb.rd(abr::OFF_MLDSA_SIGNATURE);
    tb.check(raw_xact(tb.isock, tlm::TLM_WRITE_COMMAND, abr::OFF_MLDSA_SIGNATURE,
                      reinterpret_cast<unsigned char *>(&be_data), 4u, 4u, be0, 0u) ==
                 tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,
             "register BE length 0 returns BYTE_ENABLE_ERROR");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_SIGNATURE), sig_before_be,
                "rejected BE write leaves SIGNATURE unchanged");

    uint32_t dbg_name = 0u;
    tb.check(raw_dbg(tb.isock, tlm::TLM_READ_COMMAND, abr::OFF_MLDSA_NAME,
                     reinterpret_cast<unsigned char *>(&dbg_name), 4u) == 4u,
             "transport_dbg read returns 4 bytes");
    tb.check_eq(dbg_name, name0_snap, "transport_dbg reads the live NAME value");

    tb.check(!raw_dmi(tb.isock, abr::OFF_MLDSA_NAME),
             "register target refuses DMI (no get_direct_mem_ptr)");

    tb.section("Coverage: KV / KM transport rejects");

    tb.check(raw_xact(tb.kv_isock, tlm::TLM_WRITE_COMMAND, 0u, nullptr, 4u) ==
                 tlm::TLM_ADDRESS_ERROR_RESPONSE,
             "KV write with a null data pointer is rejected");

    // Populate entry 0, then IGNORE must leave it and the initiator buffer alone.
    const std::vector<uint8_t> kv_marker(abr::KV_ENTRY_BYTES, 0x5Au);
    tb.kv_push(0u, kv_marker);
    const std::vector<uint8_t> kv0_before = tb.kv_read(0u, abr::KV_ENTRY_BYTES);
    uint32_t scratch = 0xDEADBEEFu;
    tb.check(raw_xact(tb.kv_isock, tlm::TLM_IGNORE_COMMAND, 0u,
                      reinterpret_cast<unsigned char *>(&scratch), 4u) ==
                 tlm::TLM_OK_RESPONSE,
             "KV IGNORE command returns OK");
    tb.check(tb.kv_read(0u, abr::KV_ENTRY_BYTES) == kv0_before,
             "KV IGNORE leaves the destination entry unchanged");
    tb.check_eq(scratch, 0xDEADBEEFu, "KV IGNORE leaves the initiator buffer unchanged");

    // Zero-length KV write: handler currently accepts in-window len=0 as OK.
    uint8_t kv_z = 0u;
    tb.check(raw_xact(tb.kv_isock, tlm::TLM_WRITE_COMMAND, 0u, &kv_z, 0u) ==
                 tlm::TLM_OK_RESPONSE,
             "KV zero-length write returns OK (no bytes transferred)");
    tb.check(tb.kv_read(0u, abr::KV_ENTRY_BYTES) == kv0_before,
             "KV zero-length write does not mutate the entry");

    tb.check(raw_xact(tb.km_mldsa_seed_isock, tlm::TLM_WRITE_COMMAND, 0u, nullptr, 4u) ==
                 tlm::TLM_ADDRESS_ERROR_RESPONSE,
             "KM share write with a null data pointer is rejected");
    uint32_t km_short = 0u;
    tb.check(raw_xact(tb.km_mldsa_seed_isock, tlm::TLM_WRITE_COMMAND, 0u,
                      reinterpret_cast<unsigned char *>(&km_short), 2u) ==
                 tlm::TLM_ADDRESS_ERROR_RESPONSE,
             "KM share write shorter than 4 bytes is rejected");
    tb.check(raw_xact(tb.km_mldsa_seed_isock, tlm::TLM_WRITE_COMMAND, 0u,
                      reinterpret_cast<unsigned char *>(&km_short), 0u) ==
                 tlm::TLM_ADDRESS_ERROR_RESPONSE,
             "KM share zero-length write is rejected");

    tb.section("Coverage: load_kv_entry short material");

    const abr::bytes short_mat{0x11u, 0x22u, 0x33u};
    tb.dut.load_kv_entry(5u, short_mat);
    const std::vector<uint8_t> loaded = tb.kv_read(5u, abr::KV_ENTRY_BYTES);
    tb.check(loaded[0] == 0x11u && loaded[1] == 0x22u && loaded[2] == 0x33u,
             "short load_kv_entry copies the provided prefix");
    tb.check(loaded[3] == 0u, "short load_kv_entry zero-fills the rest of the slot");

    tb.section("Coverage: KV ML-KEM empty-entry failure");

    tb.wr(abr::OFF_KV_MLKEM_MSG_RD_CTRL, KV_READ_EN | (31u << KV_ENTRY_SHIFT));
    const uint32_t msg_fail = tb.rd(abr::OFF_KV_MLKEM_MSG_RD_STATUS);
    tb.check_eq(msg_fail & KV_STATUS_VALID, 0u, "empty ML-KEM MSG KV entry is not VALID");
    tb.check_eq((msg_fail >> KV_ERROR_SHIFT) & KV_ERROR_MASK, abr::KV_READ_FAIL,
                "empty ML-KEM MSG KV entry reports KV_READ_FAIL");

    tb.wr(abr::OFF_KV_MLKEM_SEED_RD_CTRL, KV_READ_EN | (30u << KV_ENTRY_SHIFT));
    tb.check_eq((tb.rd(abr::OFF_KV_MLKEM_SEED_RD_STATUS) >> KV_ERROR_SHIFT) & KV_ERROR_MASK,
                abr::KV_READ_FAIL, "empty ML-KEM seed KV entry reports KV_READ_FAIL");

    tb.section("Coverage: KV-sourced fused commands");

    tb.check(tb.zeroize_mldsa(), "zeroize before KV KEYGEN+SIGN");
    tb.kv_push(2u, std::vector<uint8_t>(abr::MLDSA_SEED_BYTES, 0x7Au));
    tb.wr(abr::OFF_KV_MLDSA_SEED_RD_CTRL, KV_READ_EN | (2u << KV_ENTRY_SHIFT));
    tb.check(tb.run_mldsa(CMD_KEYGEN_SIGN), "KEYGEN+SIGN with a KV seed completes");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_PUBKEY), 0xABABABABu,
                "KV-seeded KEYGEN+SIGN used the stub public key");

    tb.check(tb.zeroize_mlkem(), "zeroize before KV KEYGEN+DECAPS");
    tb.kv_push(6u, std::vector<uint8_t>(abr::MLKEM_SEED_BYTES, 0x6Bu));
    tb.wr(abr::OFF_KV_MLKEM_SEED_RD_CTRL, KV_READ_EN | (6u << KV_ENTRY_SHIFT));
    tb.check(tb.run_mlkem(CMD_KEYGEN_DECAPS), "KEYGEN+DECAPS with a KV seed completes");
    tb.check_eq(tb.rd(abr::OFF_MLKEM_ENCAPS_KEY), 0x33333333u,
                "KV-seeded KEYGEN+DECAPS used the stub encapsulation key");

    tb.check(tb.zeroize_mlkem(), "zeroize before KV-sourced ENCAPS");
    tb.kv_push(8u, std::vector<uint8_t>(abr::MLKEM_MSG_BYTES, 0x5Cu));
    tb.wr(abr::OFF_KV_MLKEM_MSG_RD_CTRL, KV_READ_EN | (8u << KV_ENTRY_SHIFT));
    tb.check_eq(tb.rd(abr::OFF_KV_MLKEM_MSG_RD_STATUS) & KV_STATUS_VALID, KV_STATUS_VALID,
                "ML-KEM MSG KV read reports VALID");
    tb.check(tb.run_mlkem(CMD_ENCAPS), "ENCAPS with a KV-sourced message completes");
    tb.check_eq(tb.rd(abr::OFF_MLKEM_SHARED_KEY), 0x66666666u,
                "KV-sourced ENCAPS used the stub shared key");

    tb.section("Coverage: STREAM_MSG empty / strobe-0 / verify modifiers");

    tb.check(tb.zeroize_mldsa(), "zeroize before empty STREAM_MSG");
    tb.check(tb.run_mldsa(CMD_SIGN | CTRL_STREAM_MSG),
             "SIGN with STREAM_MSG and an empty stream falls back to MSG");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_SIGNATURE), 0xEFEFEFEFu,
                "empty-stream SIGN published the stub signature");

    tb.check(tb.zeroize_mldsa(), "zeroize before strobe-0 stream");
    tb.wr(abr::OFF_MLDSA_MSG_STROBE, 0u);
    tb.wr(abr::OFF_MLDSA_MSG, 0xAABBCCDDu);
    tb.check(tb.run_mldsa(CMD_SIGN | CTRL_STREAM_MSG),
             "SIGN with MSG_STROBE=0 adds no stream bytes");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_SIGNATURE), 0xEFEFEFEFu,
                "strobe-0 SIGN published the stub signature (disabled lanes excluded)");

    tb.check(tb.zeroize_mldsa(), "zeroize before VERIFY with context");
    tb.wr(abr::OFF_MLDSA_CTX_CONFIG, 4u);
    tb.wr(abr::OFF_MLDSA_CTX, 0x11223344u);
    tb.check(tb.run_mldsa(CMD_VERIFY), "VERIFY with a non-empty context completes");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_VERIFY_RES), 0x11111111u,
                "VERIFY with context published the stub c-tilde");

    tb.check(tb.zeroize_mldsa(), "zeroize before streamed VERIFY");
    tb.wr(abr::OFF_MLDSA_MSG_STROBE, 0xFu);
    tb.wr(abr::OFF_MLDSA_MSG, 0x01020304u);
    tb.check(tb.run_mldsa(CMD_VERIFY | CTRL_STREAM_MSG),
             "VERIFY with STREAM_MSG completes");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_VERIFY_RES), 0x11111111u,
                "streamed VERIFY published the stub c-tilde");

    tb.section("Coverage: streamed-message cap");

    tb.check(tb.zeroize_mldsa(), "zeroize before stream-cap fill");
    tb.wr(abr::OFF_MLDSA_MSG_STROBE, 0xFu);
    // Cap is 65536 B; one write past that takes the drop path. Stub SIGN keeps
    // this cheap — do not add a second FIPS-cost fill.
    for (unsigned int i = 0; i < (65536u / 4u) + 1u; ++i) {
        tb.wr(abr::OFF_MLDSA_MSG, i);
    }
    tb.check(tb.run_mldsa(CMD_SIGN | CTRL_STREAM_MSG),
             "SIGN after the stream cap still completes");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_SIGNATURE), 0xEFEFEFEFu,
                "stream-cap SIGN published the stub signature word");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & ST_READY, 0u,
                "stream-cap SIGN parks READY low");

    tb.section("Coverage: short backend outputs");

    tb.dut.set_crypto_backend(std::make_unique<short_backend>());
    tb.reset_dut();
    tb.check(tb.run_mldsa(CMD_KEYGEN), "KEYGEN with undersized backend outputs completes");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_PUBKEY), 0x00111111u,
                "short pubkey is little-endian packed and zero-padded");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_PUBKEY + 4u), 0u,
                "bytes beyond the short pubkey are zero");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_PRIVKEY_OUT), 0x00222222u,
                "short privkey is little-endian packed and zero-padded");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_PRIVKEY_OUT + 4u), 0u,
                "bytes beyond the short privkey are zero");
    tb.check(tb.zeroize_mldsa(), "zeroize after short KEYGEN");
    tb.check(tb.run_mldsa(CMD_SIGN), "SIGN with undersized backend outputs completes");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_SIGNATURE), 0x00003333u,
                "short signature packs 2 bytes little-endian");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_SIGNATURE + 4u), 0u,
                "bytes beyond the short signature are zero");
    tb.check(tb.zeroize_mldsa(), "zeroize after short SIGN");
    tb.check(tb.run_mldsa(CMD_VERIFY), "VERIFY with undersized backend outputs completes");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_VERIFY_RES), 0x00000044u,
                "short verify c-tilde packs 1 byte");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_VERIFY_RES + 4u), 0u,
                "bytes beyond the short verify result are zero");
    tb.check(tb.zeroize_mlkem(), "zeroize ML-KEM before short KEYGEN");
    tb.check(tb.run_mlkem(CMD_KEYGEN), "ML-KEM KEYGEN with undersized outputs completes");
    tb.check_eq(tb.rd(abr::OFF_MLKEM_ENCAPS_KEY), 0x00006666u,
                "short ML-KEM encaps key packs 2 bytes");
    tb.check_eq(tb.rd(abr::OFF_MLKEM_ENCAPS_KEY + 4u), 0u,
                "bytes beyond the short encaps key are zero");
    tb.check(tb.zeroize_mlkem(), "zeroize before short ENCAPS");
    tb.check(tb.run_mlkem(CMD_ENCAPS), "ENCAPS with undersized outputs completes");
    tb.check_eq(tb.rd(abr::OFF_MLKEM_CIPHERTEXT), 0x00008888u,
                "short ciphertext packs 2 bytes");
    tb.check_eq(tb.rd(abr::OFF_MLKEM_CIPHERTEXT + 4u), 0u,
                "bytes beyond the short ciphertext are zero");
    tb.check_eq(tb.rd(abr::OFF_MLKEM_SHARED_KEY), 0x00009999u,
                "short shared key packs 2 bytes");
    tb.check_eq(tb.rd(abr::OFF_MLKEM_SHARED_KEY + 4u), 0u,
                "bytes beyond the short shared key are zero");
    tb.check(tb.zeroize_mlkem(), "zeroize before short DECAPS");
    tb.check(tb.run_mlkem(0x3u), "DECAPS with undersized outputs completes");
    tb.check_eq(tb.rd(abr::OFF_MLKEM_SHARED_KEY), 0x0000AAAAu,
                "short DECAPS shared key packs 2 bytes");
    tb.check_eq(tb.rd(abr::OFF_MLKEM_SHARED_KEY + 4u), 0u,
                "bytes beyond the short DECAPS shared key are zero");

    tb.section("Coverage: backend APIs called directly");

    abr::bytes empty;
    abr::bytes xof;
    abr::shake256(xof, 0u, empty);
    tb.check(xof.empty(), "shake256 with outlen=0 leaves an empty buffer");
    abr::shake256(xof, 8u, empty);
    tb.check(xof.size() == 8u, "shake256 on an empty input still produces output");

    abr::abr_fips_backend fips;
    abr::bytes seed(abr::MLDSA_SEED_BYTES, 0x01u);
    abr::bytes pk;
    abr::bytes sk;
    fips.mldsa_keygen(seed, pk, sk);

    abr::bytes mu;
    fips.mldsa_compute_mu(pk, empty, empty, mu);
    tb.check(mu.size() == abr::MLDSA_MU_BYTES, "FIPS compute_mu accepts empty ctx and msg");
    abr::bytes mu_sk;
    fips.mldsa_compute_mu_from_sk(sk, empty, empty, mu_sk);
    tb.check(mu_sk.size() == abr::MLDSA_MU_BYTES,
             "FIPS compute_mu_from_sk accepts empty ctx and msg");

    abr::bytes short_sig(16u, 0u);
    abr::bytes ctilde;
    fips.mldsa_verify(pk, mu, short_sig, ctilde);
    tb.check(ctilde.size() == abr::MLDSA_CTILDE_BYTES,
             "FIPS verify of a short signature still writes c-tilde");

    uint8_t c_mu[64]{};
    tb.check(abr_mldsa87_compute_mu(pk.data(), nullptr, 0u, nullptr, 0u, c_mu) == 0,
             "C wrapper compute_mu with null ctx/msg succeeds");
    tb.check(abr_mldsa87_compute_mu_from_sk(sk.data(), nullptr, 0u, nullptr, 0u, c_mu) == 0,
             "C wrapper compute_mu_from_sk with null ctx/msg succeeds");

    abr::abr_shake_backend shake;
    abr::bytes shake_pk;
    abr::bytes shake_sk;
    shake.mldsa_keygen(seed, shake_pk, shake_sk);
    abr::bytes shake_mu;
    shake.mldsa_compute_mu(shake_pk, empty, empty, shake_mu);
    tb.check(shake_mu.size() == abr::MLDSA_MU_BYTES, "SHAKE compute_mu accepts empty ctx/msg");

    tb.dut.set_crypto_backend(std::make_unique<abr::abr_fips_backend>());
    tb.reset_dut();
}
