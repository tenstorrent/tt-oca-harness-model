/*
 * This Synopsys software and associated documentation (hereinafter the
 * "Software") is an unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you. The
 * Software IS NOT an item of Licensed Software or a Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Products
 * with Synopsys or any supplement thereto. Synopsys is a registered trademark
 * of Synopsys, Inc. Other names included in the SOFTWARE may be the
 * trademarks of their respective owners.
 *
 * The contents of this file are dual-licensed; you may select either version
 * 2 of the GNU General Public License ("GPL") or the BSD-3-Clause license
 * ("BSD-3-Clause"). The GPL is included in the COPYING file accompanying the
 * SOFTWARE. The BSD License is copied below.
 *
 * BSD-3-Clause License:
 * Copyright (c) 2020 Synopsys, Inc. and/or its affiliates.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer, without
 *    modification.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The names of the above-listed copyright holders may not be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef PKA_DEFINES_H
#define PKA_DEFINES_H

#include <stdint.h>

/**
 * @{
 *  \ingroup A_PKLibDefs
 *
 * \details
 * Used to indicate maximum number of tries when
 * polling the core for an operation completion.
 *
 */
#define PKA_MAX_TRIES_ON_WAIT 50000
/**
 * @}
 */

/**
 * @{
 *  \ingroup A_PKLibDefs
 *
 * \details
 * Used to indicate when the entry point is not present in the PKA firmware build.
 *
 */
#define PKA_ENTRY_POINT_UNSUPPORTED -1
/**
 * @}
 */


/**
 * @{
 *  \ingroup A_PKLibDefs
 *
 * \details
 * The maximum size in bytes of the largest supported curve (P521).
 *
 */
#define PKA_CURVE_SIZE_MAX 66
/**
 * @}
 */


/**
 * @{
 *  \ingroup A_PKLibDefs
 *
 * \details
 * The size in bits of the supported RSA key size.
 *
 */
#define PKA_RSA_KEY_SIZE_BITS 3072
/**
 * @}
 */

/**
 * @{
 * \ingroup A_PKLibDefs
 * \details
 * The size in bytes of the supported RSA key size.
*/
#define PKA_RSA_KEY_SIZE_BYTES (PKA_RSA_KEY_SIZE_BITS / 8)

/**
 * @{
 *  \ingroup A_PKLibDefs
 *
 * \details
 * Supported ECC curve types for NIST and Brainpool. The key size for each curve is the same as the
 * curve parameter. For example curve P_256 the number of bits is 256 or 32 bytes. For the P_521 curve
 * the size is 66 bytes. All curve data is stored big-endian. C25519 curves have their own APIs.
 *
 */

enum eccCurve{
   PKA_SW_CURVE_NIST_P256  = 0,     ///< NIST 256 bits.
   PKA_SW_CURVE_NIST_P384,          ///< NIST 384 bits
   PKA_SW_CURVE_NIST_P521,          ///< NIST 521 bits
   PKA_SW_CURVE_BRAINPOOL_P256R1,   ///< Brainpool 256 bits
   PKA_SW_CURVE_BRAINPOOL_P384R1,   ///< Brainpool 384 bits
   PKA_SW_CURVE_BRAINPOOL_P512R1,   ///< Brainpool 512 bits
   PKA_SW_CURVE_SM2_SCA256,         ///< SM2 256 bits
   PKA_SW_CURVE_ED25519,            ///< Edwards 256 bits
   PKA_SW_CURVE_CURVE25519,         ///< Montgomery 256 bits
   PKA_SW_CURVE_MAX
};
/**
 * @}
 */

/**
 * @{
 *  \ingroup A_PKLibDefs
 *
 * \details
 * ECC curve sizes in bytes used in the PKA APIs
 *
 */
enum eccCurveByteSize{
   PKA_CURVE_NIST_P256_BYTE  = 32,          ///<  32 bytes.
   PKA_CURVE_NIST_P384_BYTE  = 48,          ///<  48 bytes
   PKA_CURVE_NIST_P521_BYTE   = 66,         ///<  66 bytes
   PKA_CURVE_BRAINPOOL_P256R1_BYTE = 32,    ///<  32 bytes
   PKA_CURVE_BRAINPOOL_P384R1_BYTE = 48,    ///<  48 bytes
   PKA_CURVE_BRAINPOOL_P512R1_BYTE = 64,    ///<  64 bytes
   PKA_CURVE_SM2_SCA256_BYTE = 32,          ///<  32 bytes
   PKA_CURVE_ED25519_BYTE = 32 ,            ///<  32 bytes
   PKA_CURVE_CURVE25519_BYTE = 32           ///<  32 bytes
};
/**
 * @}
 */
/**
 * @{
 *  \ingroup A_PKLibDefs
 *
 * \details
 * ECC curve sizes used in the PKA APIs
 *
 */
static const uint8_t pka_curve_size[PKA_SW_CURVE_MAX] = {
   PKA_CURVE_NIST_P256_BYTE,
   PKA_CURVE_NIST_P384_BYTE,
   PKA_CURVE_NIST_P521_BYTE,
   PKA_CURVE_BRAINPOOL_P256R1_BYTE,
   PKA_CURVE_BRAINPOOL_P384R1_BYTE,
   PKA_CURVE_BRAINPOOL_P512R1_BYTE,
   PKA_CURVE_SM2_SCA256_BYTE,
   PKA_CURVE_ED25519_BYTE,
   PKA_CURVE_CURVE25519_BYTE};
/**
 * @}
 */


/**
 * @{
 *  \ingroup A_PKLibDefs
 *
 * \details
 * Error definitions returned by the PKA APIs
 *
 */
enum errorType{
   PKA_OK = 0,                     ///< 0  Successful
   // Hardware detected failures
   PKA_INVALID_OP_CODE,            ///< 1 Hardware detected failure
   PKA_STACK_UNDERFLOW,            ///< 2 Hardware detected failure
   PKA_STACK_OVERFLOW,             ///< 3 Hardware detected failure
   PKA_WATCHDOG,                   ///< 4 Hardware detected failure
   PKA_HOST_REQUEST,               ///< 5 Hardware detected failure
   PKA_RESERVED_0,                 ///< 6 Hardware detected failure
   PKA_RESERVED_1,                 ///< 7 Hardware detected failure
   PKA_MEM_PORT_COLLISION,         ///< 8 Hardware detected failure
   PKA_RESERVED_2,                 ///< 9 Hardware detected failure
   // Firmware detected failures
   PKA_RC_REASON_INVALID_KEY = 66, ///< 66 Hardware detected failure
   PKA_RC_REASON_INVALID_POINT,    ///< 67 Hardware detected failure
   // Software failures
   PKA_INVPARAM = 80,              ///< 80 Invalid parameter
   PKA_VERFYFAIL,                  ///< 81 Verify failed
   PKA_INVRNGE,                    ///< 82 Input range failure
   PKA_ERR,                        ///< 83 Internal error
   PKA_TIMEOUT,                    ///< 84 Operation timed out
   PKA_SHA_ERR,                    ///< 85 SHA 512 error
   PKA_PVER_ERR,                   ///< 86 Point verification error
   PKA_FW_ENTRY_POINT_ERR,         ///< 87 Firmware entry point unsupported
   PKA_SM3_ERR,                    ///< 88 HASH SM3 error
   PKA_SM2_ERR,                    ///< 89 SM2 error
   PKA_LOCKFAIL,                   ///< 90 Lock fail
   PKA_EOF                         ///< 91 EOF (End Of File)
};

/**
 * @}
 */
enum hashType{
   PKA_HASH_SHA224 = 0,
   PKA_HASH_SHA256,
   PKA_HASH_SHA384,
   PKA_HASH_SHA512
};

/**
 * @{
 *  \ingroup A_PKLibDefs
 *
 * \details
 * Used in the encrypt, decrypt PKA APIs
 *
 */
#define TAG_SIZE   64
/**
 * @}
 */
/**
 * @{
 *  \ingroup A_PKLibDefs
 *
 * \details
 * Used to enable(1)/disable(0) blinding
 *
 */
#define ECC_ENABLE_BLINDING  1
/**
 * @}
 */
/**
 * @{
 *  \ingroup A_PKLibDefs
 *
 * \details
 * Used in SM2 Key Agreement
 *
 */
enum sm2_key_agree_role
   { SM2_INITIATOR,
     SM2_RESPONDER
   };
/**
 * @}
 */
/**
 * @{
 *  \ingroup A_PKLibDefs
 *
 * \details All RSA operations require either a public or a private key. These will be kept in the following structures
 * The contents are dynamic in size according to the size of the modulus chosen.
 * This structure holds a modulus and its associated precomputed values.
 * The user will initially set the modulus and the other values will be set after the precompute function is run.
 *
 */
struct pka_rsa_mod {
   uint8_t *m;
   uint8_t *r_sqr;
   uint8_t *mp;
};
/*
* This structure holds a basic RSA key, which can be a public or private key, consisting of a modulus and exponent.
* mod_size indicates the size of the modulus in bytes, and all buffer sizes are based on this value.
*
*/
struct pka_rsa_key {
   uint16_t mod_size;
   struct pka_rsa_mod n;
   uint8_t *exp;
};
/*
* This structure holds a CRT formatted RSA key. Again, mod_size indicates the size of the modulus in bytes.
* and all buffer sizes are based on this value..
*
*/
struct pka_rsa_crt_key {
   uint16_t mod_size;
   struct pka_rsa_mod p;
   struct pka_rsa_mod q;
   uint8_t *dp;
   uint8_t *dq;
   uint8_t *qinv;
};

enum pka_rsa_key_format {
   RSA_KEY=1,
   RSA_CRT_KEY
};
/**
 * @}
 */

/*
 * \details
 * Used for SM2 to select the range of the key.
 */
enum sm2_key_type {
   SM2_SIGNING_KEY,      // (1 to order - 2)
   SM2_KEY_AGREEMENT_KEY // (1 to order - 1)
};

/**
* @{
*  \ingroup A_PKLibDefs
*
* \details
* pka structures
*/
/**
* \details
* Holds the internal configuration and state information for the PKA. Populated during the initialization process.
*/
struct pka_state {
   uint32_t *base;     // PKA hw base address
   uint32_t build_cfg; // PKA hw build config
};
/**
 * @}
 */
/**
* \details
* Curve point, which is a pair of big integer coordinates whose size is defined by the size of the prime field of the curve.
*/
struct ecc_point {
   uint8_t *x;
   uint8_t *y;
};


#endif
