// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file abr_base.cpp
 * @brief Reset behaviour for the Adams Bridge register block.
 */

#include "abr_base.h"

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
