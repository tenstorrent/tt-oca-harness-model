#include "sep_efuse_base.h"

void sep_efuse_base::reset_all_registers()
{
    LOCKS_LO.reset();
    LOCKS_HI.reset();
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
    for (size_t i = 0; i < 8; i++) SIP_PUBK[i].reset();
    for (size_t i = 0; i < 8; i++) SIP_UID[i].reset();
    for (size_t i = 0; i < 8; i++) SYS_PUBK[i].reset();
    for (size_t i = 0; i < 8; i++) SYS_UID[i].reset();
    STATUS_RPT.reset();
    SEP_ROM_CTRL.reset();
    SEP_SPI_CTRL_FIELD_EN.reset();
    SPI_DISCOVERY_CTRL.reset();
    SPI_PHY_DQ_TIMING.reset();
    SPI_PHY_DQS_TIMING.reset();
    SPI_PHY_GATE_LPBK.reset();
    SPI_PHY_DLL_SLAVE.reset();
    SPI_PHY_DLL_MASTER.reset();
    SPI_PHY_MISC.reset();
    SPI_RB_VALID_TIME.reset();
    for (size_t i = 0; i < 8;  i++) PUBLIC_KEY_0[i].reset();
    for (size_t i = 0; i < 8;  i++) PUBLIC_KEY_1[i].reset();
    for (size_t i = 0; i < 16; i++) RESERVED_0[i].reset();
    for (size_t i = 0; i < 16; i++) RESERVED_1[i].reset();
    for (size_t i = 0; i < 16; i++) RESERVED_2[i].reset();
    for (size_t i = 0; i < 16; i++) RESERVED_3[i].reset();
    for (size_t i = 0; i < 16; i++) RESERVED_4[i].reset();
    for (size_t i = 0; i < 16; i++) RESERVED_5[i].reset();
    for (size_t i = 0; i < 16; i++) RESERVED_6[i].reset();
    for (size_t i = 0; i < 16; i++) RESERVED_7[i].reset();
    for (size_t i = 0; i < 8;  i++) RESERVED_LAST_256[i].reset();
    RESERVED_LAST_64_LO.reset();
    RESERVED_LAST_64_HI.reset();
    RESERVED_LAST_32.reset();

    // EFUSE_INTERFACE_CTRL — STATUS reset=0x1 (efuse_sense_done=1)
    EFUSE_INTERFACE_CTRL_STATUS.reset();
    EFUSE_WRITE_CTRL.reset();
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
}
