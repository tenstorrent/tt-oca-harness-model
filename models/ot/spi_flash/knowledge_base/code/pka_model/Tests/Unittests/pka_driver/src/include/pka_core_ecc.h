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

#ifndef PKA_CORE_ECC_H_
#define PKA_CORE_ECC_H_

#include "pka_defines.h"
#include "pka_core.h"
#include "pka_hw.h"
#include "clp300_ram_fw.h"

/** Structure defines a ECC curve */
typedef struct {
   /** The size of the curve in octets */
   int size;

   /** number of bits in curve */
   int bits;

   /** The modulus prime that defines the field the curve is in (encoded in hex) */
   const uint8_t *m;

   /** The fields A param (hex) */
   const uint8_t *a;

   /** The fields B param (hex) */
   const uint8_t *b;

   /** The x co-ordinate of the base point on the curve (hex) */
   const uint8_t *x;

   /** The y co-ordinate of the base point on the curve (hex) */
   const uint8_t *y;

   /** The modulus prime */
   const uint8_t *mp;

   /** The r_sqr */
   const uint8_t *r_sqr;

   /** R^2 mod n */
   const uint8_t *nr;

   /** inverse of n mod R */
   const uint8_t *np;

   /** order of the curve */
   const uint8_t *n;

} ecc_set_type;

/**
* Performs a byte reverse.
*
* \param  [in]  *s     buffer pointer used for input/output
* \param  [in]  len    Length of buffer.
*/
void byte_reverse(uint8_t *s, int len);

/**
* Determines the the memory offset for a specific register.
*
* \param  [in] *state     PKA instance pointer.
* \param  [in] reg        Register.
* \param  [in] size       Byte size of curve.
*
* \return
*   - Memory offset  Success.
*   - error          PKA_INVRNGE for incorrect size.
*                    PKA_INVPARAM for incorrect register.
*/
uint16_t pka_core_ecc_reg_map(struct pka_state *state, enum pka_op_reg_type reg, uint16_t size);

/**
 * Perform a modular reduction (result = x mod m).
 *
 * \param  [in] *state    PKA instance pointer.
 * \param  [in] *x        Operand x.
 * \param  [in] *m        Modulus m.
 * \param  [in] size      Size in bytes;
 * \param  [out] *result  x mod m.
 *
 * \return
 *  - error_Type      Hardware or Software failures as defined in pka_defines.h error_Type.
 */
int pka_core_ecc_reduce(struct pka_state *state, uint8_t *x, uint8_t *m, uint8_t *result, uint16_t size);

/**
* Perform a bit-serial modular reduction (result = x mod m).
*
* \param  [in] *state    PKA instance pointer.
* \param  [in] *x        Operand.
* \param  [in] *m        Modulus m.
* \param  [in] size      Size in bytes;
* \param [out] *result  x mod m.
*
* \return
*  - error_Type      Hardware or Software failures as defined in pka_defines.h error_Type.
*/
int pka_core_ecc_mod(struct pka_state *state, uint8_t *x, uint8_t *m, uint8_t *result, uint16_t size);

/**
 * Perform a double precision bit-serial modular reduction (result = x mod m).
 *
 * \param  [in] *state    PKA instance pointer.
 * \param  [in] *x        Operand 2*size.
 * \param  [in] *m        Modulus m.
 * \param  [in] size      Size in bytes;
 * \param  [out] *result  x mod m.
 *
 * \return
 *  - error_Type      Hardware or Software failures as defined in pka_defines.h error_Type.
 */
int pka_core_ecc_mod_dp(struct pka_state *state, uint8_t *x, uint8_t *m, uint8_t *result, uint16_t size);

/**
 * Perform a modular addition (result = x + y mod m).
 *
 * \param  [in] *state    PKA instance pointer.
 * \param  [in] *x        Operand x.
 * \param  [in] xlen      Size in bytes of parameter x;
 * \param  [in] *y        Operand y.
 * \param  [in] *m        Modulus m.
 * \param  [in] size      Size in bytes;
 * \param  [out] *result  x + y  mod m.
 *
 * \return
 *  - error_Type      Hardware or Software failures as defined in pka_defines.h error_Type.
 */
int pka_core_ecc_modadd(struct pka_state *state, uint8_t *x,  uint32_t xlen, uint8_t *y, uint8_t *m, uint8_t *result, uint16_t size);

/**
 * Perform a modular subtraction (result = x - y mod m).
 *
 * \param  [in] *state    PKA instance pointer.
 * \param  [in] *x        Operand x.
 * \param  [in] *y        Operand y.
 * \param  [in] *m        Modulus m.
 * \param  [in] size      Size in bytes;
 * \param  [out] *result  x - y  mod m.
 *
 * \return
 *  - error_Type      Hardware or Software failures as defined in pka_defines.h error_Type.
 */
int pka_core_ecc_modsub(struct pka_state *state, uint8_t *x, uint8_t *y, uint8_t *m, uint8_t *result, uint16_t size);

/**
 * Perform a modular multiplication (result = x * y mod m).
 *
 * \param  [in] *state    PKA instance pointer.
 * \param  [in] *x        Operand x.
 * \param  [in] *y        Operand y.
 * \param  [in] *m        Modulus m.
 * \param  [in] *mp       Montgomery precomputed value.
 * \param  [in] *r_sqr    Montgomery precomputed value r**2 mod m.
 * \param  [in] size      Size in bytes;
 * \param  [out] *result  x * y  mod m.
 *
 * \return
 *  - error_Type      Hardware or Software failures as defined in pka_defines.h error_Type.
 */
int pka_core_ecc_modmult(struct pka_state *state, uint8_t *x, uint8_t *y, uint8_t *m, uint8_t *mp, uint8_t *r_sqr, uint8_t *result, uint16_t size);

/**
 * Perform a modular division (result = x/y mod m).
 *
 * \param  [in] *state    PKA instance pointer.
 * \param  [in] *x        Operand x.
 * \param  [in] xlen      Operand x length.
 * \param  [in] *y        Operand y.
 * \param  [in] *m        Modulus m.
 * \param  [in] size      Size of y, m in bytes;
 * \param  [out] *result  x/y  mod m.
 *
 * \return
 *  - error_Type      Hardware or Software failures as defined in pka_defines.h error_Type.
 */
int pka_core_ecc_moddiv(struct pka_state *state, uint8_t *x, uint32_t xlen, uint8_t *y, uint8_t *m, uint8_t *result, uint16_t size);

/**
 * Perform a ECC point multiplication on a private key using a public key ecc_point.
 *
 * \param  [in] *state    PKA instance pointer.
 * \param  [in] keypub    public key ecc point.
 * \param  [in] *keypriv  private key.
 * \param  [in] *b        blinding.
 * \param  [in] *m        Modulus m.
 * \param  [in] *a        Curve parameter a.
 * \param  [in] *mp       Montgomery precomputed value.
 * \param  [in] *r_sqr    Montgomery precomputed value r**2 mod m.
 * \param  [in] mod_size  Modulus size in bytes.
 * \param  [out] *secret  secret ecc_point
 *
 * \return
 *  - error_Type      Hardware or Software failures as defined in pka_defines.h error_Type.
 */
int pka_core_ecc_pmult(struct pka_state *state, struct ecc_point keypub, const uint8_t *keypriv, const uint8_t *b,
                       const uint8_t *m, const uint8_t *a, const uint8_t *mp, const uint8_t *r_sqr,
                       int mod_size, struct ecc_point secret);

/**
 * Perform a ECC-521 point multiplication on a private key using a public key ecc_point.
 *
 * \param  [in] *state    PKA instance pointer.
 * \param  [in] keypub    public key ecc point.
 * \param  [in] *keypriv  private key.
 * \param  [in] *b        blinding.
 * \param  [in] *m        Modulus m.
 * \param  [in] *a        Curve parameter a.
 * \param  [in] mod_size  Modulus size in bytes.
 * \param  [out] *secret  secret ecc_point
 *
 * \return
 *  - error_Type      Hardware or Software failures as defined in pka_defines.h error_Type.
 */
int pka_core_ecc_pmult_521(struct pka_state *state, struct ecc_point keypub, const uint8_t *keypriv, const uint8_t *b,
                           const uint8_t *m, const uint8_t *a, int mod_size, struct ecc_point secret);

/**
 * Perform a ECC point add of ecc_point p plus ecc_point q.
 *
 * \param  [in] *state    PKA instance pointer.
 * \param  [in] p         ecc_point.
 * \param  [in] q         ecc_point.
 * \param  [in] *m        Modulus m.
 * \param  [in] *a        Curve parameter a.
 * \param  [in] *mp       Montgomery precomputed value.
 * \param  [in] *r_sqr    Montgomery precomputed value r**2 mod m.
 * \param  [in] mod_size  Modulus size in bytes.
 * \param  [out] result   ecc_point.
 *
 * \return
 *  - error_Type      Hardware or Software failures as defined in pka_defines.h error_Type.
 */
int pka_core_ecc_point_add(struct pka_state *state, struct ecc_point p, struct ecc_point q,
                           const uint8_t *m, const uint8_t *a, const uint8_t *mp, const uint8_t *r_sqr,
                           int mod_size, struct ecc_point result);

/**
 * Perform a ECC-521 point add of ecc_point p plus ecc_point q.
 *
 * \param  [in] *state    PKA instance pointer.
 * \param  [in] p         ecc_point.
 * \param  [in] q         ecc_point.
 * \param  [in] *m        Modulus m.
 * \param  [in] *a        Curve parameter a.
 * \param  [in] mod_size  Modulus size in bytes.
 * \param  [out] result   ecc_point.
 *
 * \return
 *  - error_Type      Hardware or Software failures as defined in pka_defines.h error_Type.
 */
int pka_core_ecc_point_add_521(struct pka_state *state, struct ecc_point p, struct ecc_point q,
                               const uint8_t *m, const uint8_t *a, int mod_size, struct ecc_point result);

/**
 * Perform an ecc point validation.
 *
 * \param  [in] *state    PKA instance pointer.
 * \param  [in] *m        Modulus m.
 * \param  [in] *a        Curve parameter a.
 * \param  [in] *b        blinding.
 * \param  [in] *mp       Montgomery precomputed value.
 * \param  [in] *r_sqr    Montgomery precomputed value r**2 mod m.
 * \param  [in] mod_size  Modulus size in bytes.
 * \param  [in] point     ecc_point to validate.
 *
 * \return
 *  - error_Type      Hardware or Software failures as defined in pka_defines.h error_Type.
 */
int pka_core_ecc_point_validate(struct pka_state *state,
                                const uint8_t *m, const uint8_t *a, const uint8_t *b,
                                const uint8_t *mp, const uint8_t *r_sqr,
                                int mod_size, struct ecc_point point);

/**
 * Perform an ecc-521 point validation.
 *
 * \param  [in] *state    PKA instance pointer.
 * \param  [in] *m        Modulus m.
 * \param  [in] *a        Curve parameter a.
 * \param  [in] *b        blinding.
 * \param  [in] mod_size  Modulus size in bytes.
 * \param  [in] point     ecc_point to validate.
 *
 * \return
 *  - error_Type      Hardware or Software failures as defined in pka_defines.h error_Type.
 */
int pka_core_ecc_point_validate_521(struct pka_state *state,
                                    const uint8_t *m, const uint8_t *a, const uint8_t *b,
                                    int mod_size, struct ecc_point point);

/**
 * Perform Shamir's Trick
 * result = (p * key_p) + (q * key_q)
 *
 * \param  [in] *state    PKA instance pointer.
 * \param  [in] p         p ecc_point.
 * \param  [in] *key_p    key for p.
 * \param  [in] q         q ecc_point.
 * \param  [in] *key_q    key for q.
 * \param  [in] *m        Modulus m.
 * \param  [in] *a        Curve parameter a.
 * \param  [in] *mp       Montgomery precomputed value.
 * \param  [in] *r_sqr    Montgomery precomputed value r**2 mod m.
 * \param  [in] mod_size  Modulus size in bytes.
 * \param  [out] result   Result ecc_point.
 *
 * \return
 *  - error_Type      Hardware or Software failures as defined in pka_defines.h error_Type.
 */
int pka_core_pmult2add(struct pka_state *state,
                       struct ecc_point p, const uint8_t *key_p, struct ecc_point q, const uint8_t *key_q,
                       const uint8_t *m, const uint8_t *a, const uint8_t *mp, const uint8_t *r_sqr,
                       int mod_size, struct ecc_point result);

/**
 * Perform Shamir's Trick on 521-bit curves
 * result = (p * key_p) + (q * key_q)
 *
 * \param  [in] *state    PKA instance pointer.
 * \param  [in] p         p ecc_point.
 * \param  [in] *key_p    key for p.
 * \param  [in] q         q ecc_point.
 * \param  [in] *key_q    key for q.
 * \param  [in] *m        Modulus m.
 * \param  [in] *a        Curve parameter a.
 * \param  [in] mod_size  Modulus size in bytes.
 * \param  [out] result   Result ecc_point.
 *
 * \return
 *  - error_Type      Hardware or Software failures as defined in pka_defines.h error_Type.
 */
int pka_core_pmult2add_521(struct pka_state *state,
                           struct ecc_point p, const uint8_t *key_p, struct ecc_point q, const uint8_t *key_q,
                           const uint8_t *m, const uint8_t *a, int mod_size, struct ecc_point result);


#endif
