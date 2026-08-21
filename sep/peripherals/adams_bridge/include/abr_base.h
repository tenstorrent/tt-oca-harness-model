/**
 * @file abr_base.h
 * @brief Register block for the Adams Bridge (ABR) PQC engine.
 *
 * Declares every software-visible word in the 64 KiB ABR aperture and binds
 * the backing csml_memory to the TLM target socket. Behaviour lives in
 * adams_bridge.h (abr_ip); this layer is pure register declaration, matching
 * the hmac_base / kmac_base convention used by the other SEP crypto IPs.
 *
 * Byte offsets come from tt-oca-hw/vendor/adams_bridge/src/abr_top/rtl/abr_reg.rdl.
 * csml_reg takes a WORD index, hence the /sizeof(unsigned int) throughout.
 */

#pragma once

#include "abr_register.h"

#include <tlm_utils/simple_target_socket.h>

#include <string>

namespace abr {

// =============================================================================
// Byte offsets (relative to the ABR aperture base, SEP: 0x1094_0000)
//
// Shared with the functional model and the testbench so a map change lands in
// exactly one place.
// =============================================================================

// --- ML-DSA + common ---------------------------------------------------------
static constexpr unsigned int OFF_MLDSA_NAME         = 0x0000u; ///< 2 dwords, RO
static constexpr unsigned int OFF_MLDSA_VERSION      = 0x0008u; ///< 2 dwords, RO
static constexpr unsigned int OFF_MLDSA_CTRL         = 0x0010u;
static constexpr unsigned int OFF_MLDSA_STATUS       = 0x0014u;
static constexpr unsigned int OFF_ABR_ENTROPY        = 0x0018u; ///< 16 dwords, WO
static constexpr unsigned int OFF_MLDSA_SEED         = 0x0058u; ///< 8 dwords, WO
static constexpr unsigned int OFF_MLDSA_SIGN_RND     = 0x0078u; ///< 8 dwords, WO
static constexpr unsigned int OFF_MLDSA_MSG          = 0x0098u; ///< 16 dwords, WO
static constexpr unsigned int OFF_MLDSA_VERIFY_RES   = 0x00D8u; ///< 16 dwords, RO
static constexpr unsigned int OFF_MLDSA_EXTERNAL_MU  = 0x0118u; ///< 16 dwords, WO
static constexpr unsigned int OFF_MLDSA_MSG_STROBE   = 0x0158u;
static constexpr unsigned int OFF_MLDSA_CTX_CONFIG   = 0x015Cu;
static constexpr unsigned int OFF_MLDSA_CTX          = 0x0160u; ///< 64 dwords, WO
static constexpr unsigned int OFF_MLDSA_PUBKEY       = 0x1000u; ///< 648 dwords, RW
static constexpr unsigned int OFF_MLDSA_SIGNATURE    = 0x2000u; ///< 1157 dwords, RW
static constexpr unsigned int OFF_MLDSA_PRIVKEY_OUT  = 0x4000u; ///< 1224 dwords, RO
static constexpr unsigned int OFF_MLDSA_PRIVKEY_IN   = 0x6000u; ///< 1224 dwords, WO

// --- ML-DSA Key Vault --------------------------------------------------------
static constexpr unsigned int OFF_KV_MLDSA_SEED_RD_CTRL   = 0x8000u;
static constexpr unsigned int OFF_KV_MLDSA_SEED_RD_STATUS = 0x8004u;

// --- Interrupt block ---------------------------------------------------------
static constexpr unsigned int OFF_GLOBAL_INTR_EN          = 0x8100u;
static constexpr unsigned int OFF_ERROR_INTR_EN           = 0x8104u;
static constexpr unsigned int OFF_NOTIF_INTR_EN           = 0x8108u;
static constexpr unsigned int OFF_ERROR_GLOBAL_INTR       = 0x810Cu;
static constexpr unsigned int OFF_NOTIF_GLOBAL_INTR       = 0x8110u;
static constexpr unsigned int OFF_ERROR_INTERNAL_INTR     = 0x8114u;
static constexpr unsigned int OFF_NOTIF_INTERNAL_INTR     = 0x8118u;
static constexpr unsigned int OFF_ERROR_INTR_TRIG         = 0x811Cu;
static constexpr unsigned int OFF_NOTIF_INTR_TRIG         = 0x8120u;
static constexpr unsigned int OFF_ERROR_INTR_COUNT        = 0x8200u;
static constexpr unsigned int OFF_NOTIF_INTR_COUNT        = 0x8280u;
static constexpr unsigned int OFF_ERROR_INTR_COUNT_INCR   = 0x8300u;
static constexpr unsigned int OFF_NOTIF_INTR_COUNT_INCR   = 0x8304u;

// --- ML-KEM ------------------------------------------------------------------
static constexpr unsigned int OFF_MLKEM_NAME         = 0x9000u; ///< 2 dwords, RO
static constexpr unsigned int OFF_MLKEM_VERSION      = 0x9008u; ///< 2 dwords, RO
static constexpr unsigned int OFF_MLKEM_CTRL         = 0x9010u;
static constexpr unsigned int OFF_MLKEM_STATUS       = 0x9014u;
static constexpr unsigned int OFF_MLKEM_SEED_D       = 0x9018u; ///< 8 dwords, WO
static constexpr unsigned int OFF_MLKEM_SEED_Z       = 0x9038u; ///< 8 dwords, WO
static constexpr unsigned int OFF_MLKEM_SHARED_KEY   = 0x9058u; ///< 8 dwords, RO
static constexpr unsigned int OFF_MLKEM_MSG          = 0x9080u; ///< 8 dwords, WO
static constexpr unsigned int OFF_MLKEM_DECAPS_KEY   = 0xA000u; ///< 792 dwords, RW
static constexpr unsigned int OFF_MLKEM_ENCAPS_KEY   = 0xB000u; ///< 392 dwords, RW
static constexpr unsigned int OFF_MLKEM_CIPHERTEXT   = 0xB800u; ///< 392 dwords, RW

// --- ML-KEM Key Vault --------------------------------------------------------
static constexpr unsigned int OFF_KV_MLKEM_SEED_RD_CTRL        = 0xC000u;
static constexpr unsigned int OFF_KV_MLKEM_SEED_RD_STATUS      = 0xC004u;
static constexpr unsigned int OFF_KV_MLKEM_MSG_RD_CTRL         = 0xC008u;
static constexpr unsigned int OFF_KV_MLKEM_MSG_RD_STATUS       = 0xC00Cu;
static constexpr unsigned int OFF_KV_MLKEM_SHAREDKEY_WR_CTRL   = 0xC010u;
static constexpr unsigned int OFF_KV_MLKEM_SHAREDKEY_WR_STATUS = 0xC014u;

// --- Array element counts ----------------------------------------------------
static constexpr unsigned int N_NAME             = 2u;
static constexpr unsigned int N_VERSION          = 2u;
static constexpr unsigned int N_ENTROPY          = 16u;   ///< 512-bit SCA entropy
static constexpr unsigned int N_MLDSA_SEED       = 8u;    ///< 256-bit
static constexpr unsigned int N_SIGN_RND         = 8u;    ///< 256-bit
static constexpr unsigned int N_MLDSA_MSG        = 16u;   ///< 512-bit
static constexpr unsigned int N_VERIFY_RES       = 16u;   ///< 512-bit c-tilde
static constexpr unsigned int N_EXTERNAL_MU      = 16u;   ///< 512-bit mu
static constexpr unsigned int N_MLDSA_CTX        = 64u;   ///< up to 255 bytes
static constexpr unsigned int N_MLDSA_PUBKEY     = 648u;  ///< 2592 B ML-DSA-87 pk
static constexpr unsigned int N_MLDSA_SIGNATURE  = 1157u; ///< 4628 B
static constexpr unsigned int N_MLDSA_PRIVKEY    = 1224u; ///< 4896 B
static constexpr unsigned int N_MLKEM_SEED       = 8u;
static constexpr unsigned int N_MLKEM_SHARED_KEY = 8u;
static constexpr unsigned int N_MLKEM_MSG        = 8u;
static constexpr unsigned int N_MLKEM_DECAPS_KEY = 792u;  ///< 3168 B
static constexpr unsigned int N_MLKEM_ENCAPS_KEY = 392u;  ///< 1568 B
static constexpr unsigned int N_MLKEM_CIPHERTEXT = 392u;  ///< 1568 B

/// Full aperture size (sep_crypto_pkg: 0x1094_0000..0x1095_0000).
static constexpr unsigned int ABR_APERTURE_SIZE = 0x10000u;

/// Convert a byte offset to the word index csml_reg expects.
static constexpr unsigned int w(unsigned int byte_offset)
{
    return byte_offset / static_cast<unsigned int>(sizeof(unsigned int));
}

} // namespace abr

/**
 * @brief Register-declaration base for the Adams Bridge model.
 *
 * Every accessible word must own a csml callback: csml_memory reports
 * "RESERVED LOCATION" for any offset with no registered register, so the large
 * key / signature windows are declared as csml_reg_vector rather than left as
 * raw backing store.
 */
class abr_base : public sc_core::sc_module
{
  public:
    typedef typename csml_reg<32>::DT DT;

    abr_base(sc_core::sc_module_name name, unsigned int memory_size)
        : sc_core::sc_module(name),
          memory(std::string(name) + ".Memory", memory_size / sizeof(unsigned int)),

          // --- ML-DSA identity / control -------------------------------------
          MLDSA_NAME(std::string(name) + ".MLDSA_NAME", memory,
                     abr::w(abr::OFF_MLDSA_NAME), 1),
          MLDSA_VERSION(std::string(name) + ".MLDSA_VERSION", memory,
                        abr::w(abr::OFF_MLDSA_VERSION), 1),
          MLDSA_CTRL(std::string(name) + ".MLDSA_CTRL", memory,
                     abr::w(abr::OFF_MLDSA_CTRL)),
          MLDSA_STATUS(std::string(name) + ".MLDSA_STATUS", memory,
                       abr::w(abr::OFF_MLDSA_STATUS)),

          // --- ML-DSA inputs --------------------------------------------------
          ABR_ENTROPY(std::string(name) + ".ABR_ENTROPY", memory,
                      abr::w(abr::OFF_ABR_ENTROPY), 1),
          MLDSA_SEED(std::string(name) + ".MLDSA_SEED", memory,
                     abr::w(abr::OFF_MLDSA_SEED), 1),
          MLDSA_SIGN_RND(std::string(name) + ".MLDSA_SIGN_RND", memory,
                         abr::w(abr::OFF_MLDSA_SIGN_RND), 1),
          MLDSA_MSG(std::string(name) + ".MLDSA_MSG", memory,
                    abr::w(abr::OFF_MLDSA_MSG), 1),
          MLDSA_VERIFY_RES(std::string(name) + ".MLDSA_VERIFY_RES", memory,
                           abr::w(abr::OFF_MLDSA_VERIFY_RES), 1),
          MLDSA_EXTERNAL_MU(std::string(name) + ".MLDSA_EXTERNAL_MU", memory,
                            abr::w(abr::OFF_MLDSA_EXTERNAL_MU), 1),
          MLDSA_MSG_STROBE(std::string(name) + ".MLDSA_MSG_STROBE", memory,
                           abr::w(abr::OFF_MLDSA_MSG_STROBE)),
          MLDSA_CTX_CONFIG(std::string(name) + ".MLDSA_CTX_CONFIG", memory,
                           abr::w(abr::OFF_MLDSA_CTX_CONFIG)),
          MLDSA_CTX(std::string(name) + ".MLDSA_CTX", memory,
                    abr::w(abr::OFF_MLDSA_CTX), 1),

          // --- ML-DSA key / signature windows ---------------------------------
          MLDSA_PUBKEY(std::string(name) + ".MLDSA_PUBKEY", memory,
                       abr::w(abr::OFF_MLDSA_PUBKEY), 1),
          MLDSA_SIGNATURE(std::string(name) + ".MLDSA_SIGNATURE", memory,
                          abr::w(abr::OFF_MLDSA_SIGNATURE), 1),
          MLDSA_PRIVKEY_OUT(std::string(name) + ".MLDSA_PRIVKEY_OUT", memory,
                            abr::w(abr::OFF_MLDSA_PRIVKEY_OUT), 1),
          MLDSA_PRIVKEY_IN(std::string(name) + ".MLDSA_PRIVKEY_IN", memory,
                           abr::w(abr::OFF_MLDSA_PRIVKEY_IN), 1),

          // --- ML-DSA Key Vault ------------------------------------------------
          kv_mldsa_seed_rd_ctrl(std::string(name) + ".kv_mldsa_seed_rd_ctrl", memory,
                                abr::w(abr::OFF_KV_MLDSA_SEED_RD_CTRL)),
          kv_mldsa_seed_rd_status(std::string(name) + ".kv_mldsa_seed_rd_status", memory,
                                  abr::w(abr::OFF_KV_MLDSA_SEED_RD_STATUS)),

          // --- Interrupt block ---------------------------------------------------
          global_intr_en_r(std::string(name) + ".global_intr_en_r", memory,
                           abr::w(abr::OFF_GLOBAL_INTR_EN)),
          error_intr_en_r(std::string(name) + ".error_intr_en_r", memory,
                          abr::w(abr::OFF_ERROR_INTR_EN)),
          notif_intr_en_r(std::string(name) + ".notif_intr_en_r", memory,
                          abr::w(abr::OFF_NOTIF_INTR_EN)),
          error_global_intr_r(std::string(name) + ".error_global_intr_r", memory,
                              abr::w(abr::OFF_ERROR_GLOBAL_INTR)),
          notif_global_intr_r(std::string(name) + ".notif_global_intr_r", memory,
                              abr::w(abr::OFF_NOTIF_GLOBAL_INTR)),
          error_internal_intr_r(std::string(name) + ".error_internal_intr_r", memory,
                                abr::w(abr::OFF_ERROR_INTERNAL_INTR)),
          notif_internal_intr_r(std::string(name) + ".notif_internal_intr_r", memory,
                                abr::w(abr::OFF_NOTIF_INTERNAL_INTR)),
          error_intr_trig_r(std::string(name) + ".error_intr_trig_r", memory,
                            abr::w(abr::OFF_ERROR_INTR_TRIG)),
          notif_intr_trig_r(std::string(name) + ".notif_intr_trig_r", memory,
                            abr::w(abr::OFF_NOTIF_INTR_TRIG)),
          error_internal_intr_count_r(std::string(name) + ".error_internal_intr_count_r",
                                      memory, abr::w(abr::OFF_ERROR_INTR_COUNT)),
          notif_cmd_done_intr_count_r(std::string(name) + ".notif_cmd_done_intr_count_r",
                                      memory, abr::w(abr::OFF_NOTIF_INTR_COUNT)),
          error_internal_intr_count_incr_r(
              std::string(name) + ".error_internal_intr_count_incr_r", memory,
              abr::w(abr::OFF_ERROR_INTR_COUNT_INCR)),
          notif_cmd_done_intr_count_incr_r(
              std::string(name) + ".notif_cmd_done_intr_count_incr_r", memory,
              abr::w(abr::OFF_NOTIF_INTR_COUNT_INCR)),

          // --- ML-KEM -------------------------------------------------------------
          MLKEM_NAME(std::string(name) + ".MLKEM_NAME", memory,
                     abr::w(abr::OFF_MLKEM_NAME), 1),
          MLKEM_VERSION(std::string(name) + ".MLKEM_VERSION", memory,
                        abr::w(abr::OFF_MLKEM_VERSION), 1),
          MLKEM_CTRL(std::string(name) + ".MLKEM_CTRL", memory,
                     abr::w(abr::OFF_MLKEM_CTRL)),
          MLKEM_STATUS(std::string(name) + ".MLKEM_STATUS", memory,
                       abr::w(abr::OFF_MLKEM_STATUS)),
          MLKEM_SEED_D(std::string(name) + ".MLKEM_SEED_D", memory,
                       abr::w(abr::OFF_MLKEM_SEED_D), 1),
          MLKEM_SEED_Z(std::string(name) + ".MLKEM_SEED_Z", memory,
                       abr::w(abr::OFF_MLKEM_SEED_Z), 1),
          MLKEM_SHARED_KEY(std::string(name) + ".MLKEM_SHARED_KEY", memory,
                           abr::w(abr::OFF_MLKEM_SHARED_KEY), 1),
          MLKEM_MSG(std::string(name) + ".MLKEM_MSG", memory,
                    abr::w(abr::OFF_MLKEM_MSG), 1),
          MLKEM_DECAPS_KEY(std::string(name) + ".MLKEM_DECAPS_KEY", memory,
                           abr::w(abr::OFF_MLKEM_DECAPS_KEY), 1),
          MLKEM_ENCAPS_KEY(std::string(name) + ".MLKEM_ENCAPS_KEY", memory,
                           abr::w(abr::OFF_MLKEM_ENCAPS_KEY), 1),
          MLKEM_CIPHERTEXT(std::string(name) + ".MLKEM_CIPHERTEXT", memory,
                           abr::w(abr::OFF_MLKEM_CIPHERTEXT), 1),

          // --- ML-KEM Key Vault ----------------------------------------------------
          kv_mlkem_seed_rd_ctrl(std::string(name) + ".kv_mlkem_seed_rd_ctrl", memory,
                                abr::w(abr::OFF_KV_MLKEM_SEED_RD_CTRL)),
          kv_mlkem_seed_rd_status(std::string(name) + ".kv_mlkem_seed_rd_status", memory,
                                  abr::w(abr::OFF_KV_MLKEM_SEED_RD_STATUS)),
          kv_mlkem_msg_rd_ctrl(std::string(name) + ".kv_mlkem_msg_rd_ctrl", memory,
                               abr::w(abr::OFF_KV_MLKEM_MSG_RD_CTRL)),
          kv_mlkem_msg_rd_status(std::string(name) + ".kv_mlkem_msg_rd_status", memory,
                                 abr::w(abr::OFF_KV_MLKEM_MSG_RD_STATUS)),
          kv_mlkem_sharedkey_wr_ctrl(std::string(name) + ".kv_mlkem_sharedkey_wr_ctrl",
                                     memory, abr::w(abr::OFF_KV_MLKEM_SHAREDKEY_WR_CTRL)),
          kv_mlkem_sharedkey_wr_status(std::string(name) + ".kv_mlkem_sharedkey_wr_status",
                                       memory, abr::w(abr::OFF_KV_MLKEM_SHAREDKEY_WR_STATUS))
    {
        memory.bind_to_socket(target_socket);
    }

    /// Restore every register to its RDL reset value.
    void reset_all_registers();

    csml_memory<32> memory;
    tlm_utils::simple_target_socket<csml_memory<32>, 32> target_socket;

    // --- ML-DSA -------------------------------------------------------------
    csml_reg_vector<abr::RO_DATA_type<32>, abr::N_NAME>            MLDSA_NAME;
    csml_reg_vector<abr::RO_DATA_type<32>, abr::N_VERSION>         MLDSA_VERSION;
    abr::MLDSA_CTRL_type<32>                                       MLDSA_CTRL;
    abr::MLDSA_STATUS_type<32>                                     MLDSA_STATUS;
    csml_reg_vector<abr::WO_DATA_type<32>, abr::N_ENTROPY>         ABR_ENTROPY;
    csml_reg_vector<abr::WO_DATA_type<32>, abr::N_MLDSA_SEED>      MLDSA_SEED;
    csml_reg_vector<abr::WO_DATA_type<32>, abr::N_SIGN_RND>        MLDSA_SIGN_RND;
    csml_reg_vector<abr::WO_DATA_type<32>, abr::N_MLDSA_MSG>       MLDSA_MSG;
    csml_reg_vector<abr::RO_DATA_type<32>, abr::N_VERIFY_RES>      MLDSA_VERIFY_RES;
    csml_reg_vector<abr::WO_DATA_type<32>, abr::N_EXTERNAL_MU>     MLDSA_EXTERNAL_MU;
    abr::MLDSA_MSG_STROBE_type<32>                                 MLDSA_MSG_STROBE;
    abr::MLDSA_CTX_CONFIG_type<32>                                 MLDSA_CTX_CONFIG;
    csml_reg_vector<abr::WO_DATA_type<32>, abr::N_MLDSA_CTX>       MLDSA_CTX;
    csml_reg_vector<abr::RW_DATA_type<32>, abr::N_MLDSA_PUBKEY>    MLDSA_PUBKEY;
    csml_reg_vector<abr::RW_DATA_type<32>, abr::N_MLDSA_SIGNATURE> MLDSA_SIGNATURE;
    csml_reg_vector<abr::RO_DATA_type<32>, abr::N_MLDSA_PRIVKEY>   MLDSA_PRIVKEY_OUT;
    csml_reg_vector<abr::WO_DATA_type<32>, abr::N_MLDSA_PRIVKEY>   MLDSA_PRIVKEY_IN;

    // --- ML-DSA Key Vault ----------------------------------------------------
    abr::KV_RD_CTRL_type<32>   kv_mldsa_seed_rd_ctrl;
    abr::KV_RD_STATUS_type<32> kv_mldsa_seed_rd_status;

    // --- Interrupt block ------------------------------------------------------
    abr::GLOBAL_INTR_EN_type<32>    global_intr_en_r;
    abr::INTR_EN_type<32>           error_intr_en_r;
    abr::INTR_EN_type<32>           notif_intr_en_r;
    abr::GLOBAL_INTR_STS_type<32>   error_global_intr_r;
    abr::GLOBAL_INTR_STS_type<32>   notif_global_intr_r;
    abr::INTERNAL_INTR_type<32>     error_internal_intr_r;
    abr::INTERNAL_INTR_type<32>     notif_internal_intr_r;
    abr::INTR_TRIG_type<32>         error_intr_trig_r;
    abr::INTR_TRIG_type<32>         notif_intr_trig_r;
    abr::INTR_COUNT_type<32>        error_internal_intr_count_r;
    abr::INTR_COUNT_type<32>        notif_cmd_done_intr_count_r;
    abr::INTR_COUNT_INCR_type<32>   error_internal_intr_count_incr_r;
    abr::INTR_COUNT_INCR_type<32>   notif_cmd_done_intr_count_incr_r;

    // --- ML-KEM ----------------------------------------------------------------
    csml_reg_vector<abr::RO_DATA_type<32>, abr::N_NAME>              MLKEM_NAME;
    csml_reg_vector<abr::RO_DATA_type<32>, abr::N_VERSION>           MLKEM_VERSION;
    abr::MLKEM_CTRL_type<32>                                         MLKEM_CTRL;
    abr::MLKEM_STATUS_type<32>                                       MLKEM_STATUS;
    csml_reg_vector<abr::WO_DATA_type<32>, abr::N_MLKEM_SEED>        MLKEM_SEED_D;
    csml_reg_vector<abr::WO_DATA_type<32>, abr::N_MLKEM_SEED>        MLKEM_SEED_Z;
    csml_reg_vector<abr::RO_DATA_type<32>, abr::N_MLKEM_SHARED_KEY>  MLKEM_SHARED_KEY;
    csml_reg_vector<abr::WO_DATA_type<32>, abr::N_MLKEM_MSG>         MLKEM_MSG;
    csml_reg_vector<abr::RW_DATA_type<32>, abr::N_MLKEM_DECAPS_KEY>  MLKEM_DECAPS_KEY;
    csml_reg_vector<abr::RW_DATA_type<32>, abr::N_MLKEM_ENCAPS_KEY>  MLKEM_ENCAPS_KEY;
    csml_reg_vector<abr::RW_DATA_type<32>, abr::N_MLKEM_CIPHERTEXT>  MLKEM_CIPHERTEXT;

    // --- ML-KEM Key Vault --------------------------------------------------------
    abr::KV_RD_CTRL_type<32>   kv_mlkem_seed_rd_ctrl;
    abr::KV_RD_STATUS_type<32> kv_mlkem_seed_rd_status;
    abr::KV_RD_CTRL_type<32>   kv_mlkem_msg_rd_ctrl;
    abr::KV_RD_STATUS_type<32> kv_mlkem_msg_rd_status;
    abr::KV_WR_CTRL_type<32>   kv_mlkem_sharedkey_wr_ctrl;
    abr::KV_WR_STATUS_type<32> kv_mlkem_sharedkey_wr_status;
};
