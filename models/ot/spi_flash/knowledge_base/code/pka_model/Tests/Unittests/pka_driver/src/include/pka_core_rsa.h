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
 * Copyright (c) 2011-2020, 2022 Synopsys, Inc. and/or its affiliates.
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

#ifndef PKA_CORE_RSA_H_
#define PKA_CORE_RSA_H_

#include "pka_defines.h"
#include "pka_core.h"

/**
*  This function uses the PKA precomputation operations to populate r_sqr and mp in the supplied modulus structure.
*
* \param  [in] state  PKA instance pointer.
* \param  [in]  n         modulus structure.
* \param  [in]  mlen      modulus size.
* \return
*   - Error code for invalid input or unable to read hardware
*/
int pka_core_rsa_precomp(struct pka_state *state, struct pka_rsa_mod *n, uint16_t mlen);

/**
* \param  [in]   state       PKA instance pointer.
* \param  [in]   key         RSA key (fully prepared).
* \param  [in]   base        Modexp base (x)
* \param  [out]  result      Result c = x^y mod n
* \return
*   - Error code for invalid input or unable to read hardware
*/
int pka_core_rsa_modexp(struct pka_state *state, struct pka_rsa_key *key, uint8_t *base, uint8_t  *result);
/**
* \param  [in]   state        PKA instance pointer.
* \param  [in]   crtkey       RSA crt key (fully prepared).
* \param  [in]   base         Modexp base (x)
* \param  [out]  result       Result c = x^y mod n
* \return
*   - Error code for invalid input or unable to read hardware
*/
int pka_core_rsa_modexp_crt(struct pka_state *state, struct pka_rsa_crt_key *crtkey, uint8_t *base, uint8_t  *result);

int pka_core_rsa_mul(struct pka_state *state, uint8_t *a, uint8_t *b, uint16_t mlen, uint8_t *c);

int pka_core_rsa_keysetup_crt(struct pka_state *state, struct pka_rsa_crt_key *pka_key,
                              uint8_t *d, uint16_t dlen);
#endif
