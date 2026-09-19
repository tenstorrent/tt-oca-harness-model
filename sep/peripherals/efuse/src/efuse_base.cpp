// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "efuse_base.h"

void efuse_base::reset_all_registers()
{
    LOCKS_LO.reset();
    LOCKS_HI.reset();
    LOCKS_SPARE.reset();
    LC_STATE.reset();
    SBOOT_DIS.reset();
    TRANSIENT_RMA_EN.reset();
    SIP_DIS_LO.reset();
    SIP_DIS_HI.reset();
    SYS_DIS_LO.reset();
    SYS_DIS_HI.reset();
    for (size_t i = 0; i < 8; i++) RMA_SIP_TOKEN[i].reset();
    for (size_t i = 0; i < 8; i++) RMA_CHIPLET_TOKEN[i].reset();
    for (size_t i = 0; i < 8; i++) CLASS_KEY[i].reset();
    CHIPLET_PUBK_REVOKE.reset();
    for (size_t i = 0; i < 8; i++) BL1_VERSION[i].reset();
    for (size_t i = 0; i < 8; i++) BL2_VERSION[i].reset();
    for (size_t i = 0; i < 8; i++) CHIPLET_UID[i].reset();
    for (size_t i = 0; i < 8; i++) SIP_PUBK_HASH0[i].reset();
    for (size_t i = 0; i < 8; i++) SIP_UID[i].reset();
    for (size_t i = 0; i < 8; i++) SYS_PUBK_HASH[i].reset();
    for (size_t i = 0; i < 8; i++) SYS_UID[i].reset();
    for (size_t i = 0; i < 8; i++) CHIPLET_PUBK_HASH0[i].reset();
    for (size_t i = 0; i < 8; i++) CHIPLET_PUBK_HASH1[i].reset();
    for (size_t i = 0; i < 8; i++) CHIPLET_PUBK_PQC_HASH0[i].reset();
    for (size_t i = 0; i < 8; i++) CHIPLET_PUBK_PQC_HASH1[i].reset();
    for (size_t i = 0; i < 8; i++) SIP_PUBK_PQC_HASH0[i].reset();
    for (size_t i = 0; i < 8; i++) SYS_PUBK_PQC_HASH[i].reset();
    for (size_t i = 0; i < 8; i++) SIP_PUBK_HASH1[i].reset();
    for (size_t i = 0; i < 8; i++) SIP_PUBK_PQC_HASH1[i].reset();
    for (size_t i = 0; i < 8; i++) SEP_CHIPLET_ID[i].reset();
    for (size_t i = 0; i < 8; i++) SEP_SIP_ID[i].reset();
    for (size_t i = 0; i < 8; i++) SEP_SYS_ID[i].reset();
    for (size_t i = 0; i < 8; i++) SPARE0[i].reset();
    for (size_t i = 0; i < 8; i++) SPARE1[i].reset();
    for (size_t i = 0; i < 8; i++) SPARE2[i].reset();
    for (size_t i = 0; i < 8; i++) SPARE3[i].reset();
    for (size_t i = 0; i < 8; i++) SPARE4[i].reset();
    for (size_t i = 0; i < 8; i++) SPARE5[i].reset();
    for (size_t i = 0; i < 8; i++) SPARE6[i].reset();
    for (size_t i = 0; i < 8; i++) SPARE7[i].reset();
    for (size_t i = 0; i < 8; i++) SPARE8[i].reset();
    REQUIRED_SIGNERS.reset();
    REQUIRED_ALGS.reset();
    STATUS_RPT.reset();
    SEP_ROM_CTRL.reset();
    SYSCLK_FREQ_MHZ.reset();

    // EFUSE_INTERFACE_CTRL — STATUS reset=0x1 (efuse_sense_done=1)
    EFUSE_INTERFACE_CTRL_STATUS.reset();
    EFUSE_PROGRAM_CTRL.reset();
    EFUSE_READ_CTRL.reset();
    EFUSE_PROGRAM_INTERFACE_RD_DATA.reset();
    EFUSE_READ_INTERFACE_RD_DATA.reset();
    EFUSE_READ_REQ_TIMEOUT.reset();
    EFUSE_PROGRAM_REQ_TIMEOUT.reset();

    // EFUSE_MMR
    for (size_t i = 0; i < 8; i++) RMA_SIP_TOKEN_I[i].reset();
    for (size_t i = 0; i < 8; i++) RMA_CHIPLET_TOKEN_I[i].reset();
    for (size_t i = 0; i < 8; i++) SEC_DISABLE_TOKEN_I[i].reset();
    TOKEN_EOP.reset();
    RMA_SIP_TOKEN_MATCH.reset();
    RMA_CHIPLET_TOKEN_MATCH.reset();
    SEC_DISABLE_TOKEN_MATCH.reset();

    // EFUSE_SHIM_CTRL — the only register in the map with a non-zero reset, so
    // unlike the stubs it replaced it has to be reset explicitly.
    EFUSE_BANK_INIT_TIME.reset();
}
