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

#ifndef PKA_CORE_MONTG_ED_H_
#define PKA_CORE_MONTG_ED_H_

#include "pka_defines.h"
#include "pka_debug.h"

/**
 * Perform a Montgomery modular multiplication (c = k * u mod m).
 *
 * \param  [in] *state    PKA instance pointer.
 * \param  [in] *k        Operand k.
 * \param  [in] *u        Operand u.
 * \param  [in] *b        blinding.
 * \param  [in] *m        Modulus m.
 * \param  [in] mod_size  Modulus size in bytes.
 * \param  [out] *c       k * u mod m.
 *
 * \return
 *  - error_Type      Hardware or Software failures as defined in pka_defines.h error_Type.
 */
int pka_core_x25519_pmult(struct pka_state *state, uint8_t *k, uint8_t *u,
                          uint8_t *b, const uint8_t *m, int mod_size, uint8_t *c);

/**
 * Perform a Edwards modular multiplication (enc_pointc = point * s mod m).
 *
 * \param  [in] *state    PKA instance pointer.
 * \param  [in] point     ecc_point x, y.
 * \param  [in] *s        s.
 * \param  [in] *b        blinding.
 * \param  [in] *m        Modulus m.
 * \param  [in] *d        Curve parameter d.
 * \param  [in] mod_size  Modulus size in bytes.
 * \param  [out] *enc_point  enc_point = point * s mod m.
 *
 * \return
 *  - error_Type      Hardware or Software failures as defined in pka_defines.h error_Type.
 */
int pka_core_eddsa_pmult(struct pka_state *state, struct ecc_point point, uint8_t *s, uint8_t *b,
                         const uint8_t *m, const uint8_t *d, int mod_size, struct ecc_point enc_point);

/**
 * Perform a Edwards point add (Result = (point p plus point q) mod m).
 *
 * \param  [in] *state    PKA instance pointer.
 * \param  [in] p         p ecc_point.
 * \param  [in] q         q ecc_point.
 * \param  [in] *b        blinding.
 * \param  [in] *m        Modulus m.
 * \param  [in] *d        Curve parameter d.
 * \param  [in] mod_size  Modulus size in bytes.
 * \param  [out] result   Result = (point p plus point q) mod m
 *
 * \return
 *  - error_Type      Hardware or Software failures as defined in pka_defines.h error_Type.
 */
int pka_core_eddsa_point_add(struct pka_state *state, struct ecc_point p, struct ecc_point q, uint8_t *b,
                             const uint8_t *m, const uint8_t *d, int mod_size, struct ecc_point result);

/**
 * Perform a Edwards Multiplication result = x * y where m is the order of the base point (OBP).
 *
 * \param  [in] *state    PKA instance pointer.
 * \param  [in] *x        x operand.
 * \param  [in] *y        y operand.
 * \param  [in] *m        Modulus m.
 * \param  [in] *mp       Montgomery precomputed value.
 * \param  [in] *r_sqr    Montgomery precomputed value r**2 mod m.
 * \param  [in] mod_size  Modulus size in bytes.
 * \param  [out] result   Result = (x * y) mod m
 *
 * \return
 *  - error_Type      Hardware or Software failures as defined in pka_defines.h error_Type.
 */
int pka_core_eddsa_modmult_obp(struct pka_state *state, uint8_t *x, uint8_t *y, const uint8_t *m,
                               const uint8_t *mp, const uint8_t *r_sqr, int mod_size, uint8_t *result);

/**
 * Perform a Edwards x-recover from the y and x-sign for a point on the Edwards curve.
 *
 *  \param  [in] *state    PKA instance pointer.
 *  \param  [in] sign      x sign.
 *  \param  [in] *y        Operand y.
 *  \param  [in] *m        Modulus m.
 *  \param  [in] *d        Curve parameter d.
 *  \param  [in] mod_size  Modulus size in bytes.
 *  \param  [out] *result_x Result_x.
 *
 *  \return
 *  - error_Type      Hardware or Software failures as defined in pka_defines.h error_Type.
 */
int pka_core_eddsa_xrecover(struct pka_state *state, uint8_t sign, uint8_t *y, const uint8_t *m,
                            const uint8_t *d, int mod_size, uint8_t *result_x);

#endif
