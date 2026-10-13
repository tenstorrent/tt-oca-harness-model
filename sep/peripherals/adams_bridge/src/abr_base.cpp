// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file abr_base.cpp
 * @brief Bus front end and reset behaviour for the Adams Bridge register block.
 */

#include "abr_base.h"

#include <algorithm>


namespace {

/// Reset every element of a regmodel::RegVector.
template <typename Vec>
void reset_vector(Vec &vec)
{
    for (unsigned int i = 0; i < vec.size(); ++i) {
        vec[i].reset();
    }
}

} // namespace

void abr_base::reset_all_registers()
{
    // --- ML-DSA ---------------------------------------------------------------
    reset_vector(MLDSA_NAME);
    reset_vector(MLDSA_VERSION);
    MLDSA_CTRL.reset();
    MLDSA_STATUS.reset();
    reset_vector(ABR_ENTROPY);
    reset_vector(MLDSA_SEED);
    reset_vector(MLDSA_SIGN_RND);
    reset_vector(MLDSA_MSG);
    reset_vector(MLDSA_VERIFY_RES);
    reset_vector(MLDSA_EXTERNAL_MU);
    MLDSA_MSG_STROBE.reset();
    MLDSA_CTX_CONFIG.reset();
    reset_vector(MLDSA_CTX);
    reset_vector(MLDSA_PUBKEY);
    reset_vector(MLDSA_SIGNATURE);
    reset_vector(MLDSA_PRIVKEY_OUT);
    reset_vector(MLDSA_PRIVKEY_IN);

    // --- ML-DSA Key Vault -------------------------------------------------------
    kv_mldsa_seed_rd_ctrl.reset();
    kv_mldsa_seed_rd_status.reset();

    // --- Interrupt block ---------------------------------------------------------
    global_intr_en_r.reset();
    error_intr_en_r.reset();
    notif_intr_en_r.reset();
    error_global_intr_r.reset();
    notif_global_intr_r.reset();
    error_internal_intr_r.reset();
    notif_internal_intr_r.reset();
    error_intr_trig_r.reset();
    notif_intr_trig_r.reset();
    error_internal_intr_count_r.reset();
    notif_cmd_done_intr_count_r.reset();
    error_internal_intr_count_incr_r.reset();
    notif_cmd_done_intr_count_incr_r.reset();

    // --- ML-KEM --------------------------------------------------------------------
    reset_vector(MLKEM_NAME);
    reset_vector(MLKEM_VERSION);
    MLKEM_CTRL.reset();
    MLKEM_STATUS.reset();
    reset_vector(MLKEM_SEED_D);
    reset_vector(MLKEM_SEED_Z);
    reset_vector(MLKEM_SHARED_KEY);
    reset_vector(MLKEM_MSG);
    reset_vector(MLKEM_DECAPS_KEY);
    reset_vector(MLKEM_ENCAPS_KEY);
    reset_vector(MLKEM_CIPHERTEXT);

    // --- ML-KEM Key Vault -----------------------------------------------------------
    kv_mlkem_seed_rd_ctrl.reset();
    kv_mlkem_seed_rd_status.reset();
    kv_mlkem_msg_rd_ctrl.reset();
    kv_mlkem_msg_rd_status.reset();
    kv_mlkem_sharedkey_wr_ctrl.reset();
    kv_mlkem_sharedkey_wr_status.reset();
}

void abr_base::word_access(tlm::tlm_command cmd, unsigned int word, uint32_t &data)
{
    tlm::tlm_generic_payload sub;
    sc_core::sc_time zero = sc_core::SC_ZERO_TIME;
    sub.set_command(cmd);
    sub.set_address(static_cast<sc_dt::uint64>(word) * 4u);
    sub.set_data_ptr(reinterpret_cast<unsigned char *>(&data));
    sub.set_data_length(4u);
    sub.set_streaming_width(4u);
    sub.set_byte_enable_ptr(nullptr);
    memory.b_transport(sub, zero);
}

void abr_base::bus_b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay)
{
    (void)delay;
    trans.set_dmi_allowed(false);
    const tlm::tlm_response_status vs = regmodel::Memory<32>::validate(trans);
    if (vs != tlm::TLM_OK_RESPONSE) {
        trans.set_response_status(vs);
        return;
    }

    const sc_dt::uint64 addr = trans.get_address();
    const unsigned int len = trans.get_data_length();
    unsigned char *data = trans.get_data_ptr();
    const bool is_read = trans.get_command() == tlm::TLM_READ_COMMAND;

    if (len > abr::ABR_BEAT_BYTES || addr + len > abr::ABR_DECODED_EXTENT) {
        if (is_read) {
            for (unsigned int i = 0; i < len; ++i) {
                data[i] = static_cast<unsigned char>(abr::ABR_DECERR_DATA >> (((addr + i) % 4u) * 8u));
            }
        }
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    // Bytes past the 8-byte beat the access starts in are not serviced; they read 0.
    if (is_read) {
        std::fill(data, data + len, static_cast<unsigned char>(0u));
    }

    const unsigned char *be = trans.get_byte_enable_ptr();
    const unsigned int be_len = trans.get_byte_enable_length();
    const sc_dt::uint64 beat_end = (addr & ~sc_dt::uint64(abr::ABR_BEAT_BYTES - 1u)) + abr::ABR_BEAT_BYTES;
    bool slverr = false;

    for (sc_dt::uint64 w = addr & ~sc_dt::uint64(3u); w < addr + len && w < beat_end; w += 4u) {
        const unsigned int word = static_cast<unsigned int>(w / 4u);
        if (is_read) {
            uint32_t value = 0u;
            if (!bus_read_masked(word)) {
                word_access(tlm::TLM_READ_COMMAND, word, value);
            }
            for (unsigned int lane = 0; lane < 4u; ++lane) {
                const sc_dt::uint64 a = w + lane;
                if (a >= addr && a < addr + len) {
                    data[a - addr] = static_cast<unsigned char>(value >> (lane * 8u));
                }
            }
            continue;
        }

        uint32_t value = 0u;
        unsigned int strobe = 0u;
        for (unsigned int lane = 0; lane < 4u; ++lane) {
            const sc_dt::uint64 a = w + lane;
            if (a < addr || a >= addr + len) {
                continue;
            }
            const unsigned int i = static_cast<unsigned int>(a - addr);
            if (be == nullptr || be[i % be_len] != 0u) {
                strobe |= 1u << lane;
                value |= static_cast<uint32_t>(data[i]) << (lane * 8u);
            }
        }
        if (strobe == 0u) {
            continue;
        }
        if (strobe != 0xFu) {
            slverr = true;
            continue;
        }
        if (bus_write_allowed(word)) {
            word_access(tlm::TLM_WRITE_COMMAND, word, value);
        }
    }

    trans.set_response_status(slverr ? tlm::TLM_GENERIC_ERROR_RESPONSE : tlm::TLM_OK_RESPONSE);
}

unsigned int abr_base::bus_transport_dbg(tlm::tlm_generic_payload &trans)
{
    return memory.transport_dbg(trans);
}
