// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file otbn_algorithm_rsa_3072.cpp
 * @brief RSA-3072 PKCS#1 v1.5 verify algorithm implementation
 */

#include "otbn_algorithm_rsa_3072.h"
#include <openssl/bn.h>
#include <openssl/err.h>
#include <cstring>
#include <algorithm>

/* DMEM byte offsets for rsa_3072_app symbols */
static constexpr size_t DMEM_N_OFFSET    = 0x000;   /* rsa_n: modulus      (384 bytes) */
static constexpr size_t DMEM_INOUT_OFFSET = 0x600;  /* inout: sig / result (384 bytes) */
static constexpr size_t RSA3072_BYTES    = 384;      /* 3072 / 8 */

/* Reverse len bytes from src into dst (converts between LE-LSWord-first and BE). */
static void reverse_bytes(const uint8_t* src, uint8_t* dst, size_t len) {
    for (size_t i = 0; i < len; ++i)
        dst[i] = src[len - 1 - i];
}

otbn_algorithm_rsa_3072::otbn_algorithm_rsa_3072(size_t dmem_size)
    : otbn_algorithm(dmem_size, false), instruction_count(63750000) {}

otbn_algorithm::status_t otbn_algorithm_rsa_3072::execute(char* dmem) {
    REG_INFO(1, logger) << "[OTBN RSA-3072] Starting execution";

    if (m_dmem_size < DMEM_INOUT_OFFSET + RSA3072_BYTES) {
        REG_ERROR(0, logger) << "[OTBN RSA-3072] ERROR: Insufficient DMEM size (" << m_dmem_size << " bytes)";
        return ERROR;
    }

    const uint8_t* db = reinterpret_cast<const uint8_t*>(dmem);
    uint8_t*       dw = reinterpret_cast<uint8_t*>(dmem);

    /* Convert from DMEM LSByte-first layout to big-endian for OpenSSL */
    uint8_t be_n  [RSA3072_BYTES];
    uint8_t be_sig[RSA3072_BYTES];
    reverse_bytes(db + DMEM_N_OFFSET,     be_n,   RSA3072_BYTES);
    reverse_bytes(db + DMEM_INOUT_OFFSET, be_sig, RSA3072_BYTES);

    BN_CTX* ctx = BN_CTX_new();
    // LCOV_EXCL_START — OpenSSL OOM / init failure cannot be injected from the TB
    if (!ctx) {
        REG_ERROR(0, logger) << "[OTBN RSA-3072] ERROR: Failed to allocate BN_CTX";
        return ERROR;
    }
    // LCOV_EXCL_STOP

    BIGNUM* n   = BN_bin2bn(be_n,   RSA3072_BYTES, nullptr);
    BIGNUM* sig = BN_bin2bn(be_sig, RSA3072_BYTES, nullptr);
    BIGNUM* e   = BN_new();
    BIGNUM* res = BN_new();

    // LCOV_EXCL_START — OpenSSL OOM
    if (!n || !sig || !e || !res) {
        REG_ERROR(0, logger) << "[OTBN RSA-3072] ERROR: Failed to allocate BIGNUMs";
        BN_free(n); BN_free(sig); BN_free(e); BN_free(res);
        BN_CTX_free(ctx);
        return ERROR;
    }
    // LCOV_EXCL_STOP

    BN_set_word(e, 65537);   /* F4 exponent hardcoded in rsa_3072_app */

    if (BN_is_zero(n)) {
        REG_ERROR(0, logger) << "[OTBN RSA-3072] ERROR: Modulus is zero";
        BN_free(n); BN_free(sig); BN_free(e); BN_free(res);
        BN_CTX_free(ctx);
        return ERROR;
    }

    // LCOV_EXCL_START — BN_mod_exp / oversized result require a mocked OpenSSL
    if (BN_mod_exp(res, sig, e, n, ctx) != 1) {
        unsigned long err = ERR_get_error();
        char err_buf[256];
        ERR_error_string_n(err, err_buf, sizeof(err_buf));
        REG_ERROR(0, logger) << "[OTBN RSA-3072] ERROR: BN_mod_exp failed: " << err_buf;
        BN_free(n); BN_free(sig); BN_free(e); BN_free(res);
        BN_CTX_free(ctx);
        return ERROR;
    }

    /* Write result back to DMEM[0x600] in LSByte-first order */
    int res_len = BN_num_bytes(res);
    if (res_len > static_cast<int>(RSA3072_BYTES)) {
        REG_ERROR(0, logger) << "[OTBN RSA-3072] ERROR: Result too large (" << res_len << " bytes)";
        BN_free(n); BN_free(sig); BN_free(e); BN_free(res);
        BN_CTX_free(ctx);
        return ERROR;
    }
    // LCOV_EXCL_STOP

    uint8_t be_res[RSA3072_BYTES] = {};
    BN_bn2bin(res, be_res + (RSA3072_BYTES - static_cast<size_t>(res_len)));

    memset(dw + DMEM_INOUT_OFFSET, 0, RSA3072_BYTES);
    reverse_bytes(be_res, dw + DMEM_INOUT_OFFSET, RSA3072_BYTES);

    REG_INFO(1, logger) << "[OTBN RSA-3072] Execution successful";

    BN_free(n); BN_free(sig); BN_free(e); BN_free(res);
    BN_CTX_free(ctx);
    return otbn_algorithm::SUCCESS;
}

uint64_t otbn_algorithm_rsa_3072::get_instruction_count() {
    return instruction_count;
}

uint64_t otbn_algorithm_rsa_3072::get_cycle_count() {
    return 10;
}

void otbn_algorithm_rsa_3072::reset() {
    instruction_count = 63750000;
}

void otbn_algorithm_rsa_3072::message_objects(std::ostream& debug, std::ostream& info) {
    debug << "[OTBN RSA-3072] Debug stream configured" << std::endl;
    info  << "[OTBN RSA-3072] Info stream configured"  << std::endl;
}
