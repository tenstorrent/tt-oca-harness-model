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
 * Copyright (c) 2020, 2023 Synopsys, Inc. and/or its affiliates.
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "pka.h"
#include "pka_core.h"
#include "pka_core_ecc.h"
#include "pka_core_montg_ed.h"
#include "sha512.h"

// PKA_SW_CURVE_ED25519  (Edwards)
static const uint8_t ed_m32[]       = {0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xed};
static const uint8_t ed_a32[]       = {0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xec};
static const uint8_t ed_d32[]       = {0x52, 0x03, 0x6c, 0xee, 0x2b, 0x6f, 0xfe, 0x73, 0x8c, 0xc7, 0x40, 0x79, 0x77, 0x79, 0xe8, 0x98, 0x00, 0x70, 0x0a, 0x4d, 0x41, 0x41, 0xd8, 0xab, 0x75, 0xeb, 0x4d, 0xca, 0x13, 0x59, 0x78, 0xa3};
static const uint8_t ed_x32[]       = {0x21, 0x69, 0x36, 0xd3, 0xcd, 0x6e, 0x53, 0xfe, 0xc0, 0xa4, 0xe2, 0x31, 0xfd, 0xd6, 0xdc, 0x5c, 0x69, 0x2c, 0xc7, 0x60, 0x95, 0x25, 0xa7, 0xb2, 0xc9, 0x56, 0x2d, 0x60, 0x8f, 0x25, 0xd5, 0x1a};
static const uint8_t ed_y32[]       = {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x58};
static const uint8_t ed_mp32[32]    = {0};
static const uint8_t ed_r_sqr32[32] = {0};
static const uint8_t ed_nr32[]      = {0x06, 0xce, 0x65, 0x04, 0x6d, 0xf0, 0xc2, 0x68, 0xf7, 0x3b, 0xb1, 0xcf, 0x48, 0x5f, 0xd6, 0xf9, 0xa0, 0x0e, 0x49, 0xd8, 0x8a, 0x62, 0x8c, 0x77, 0x8b, 0xb7, 0xda, 0x16, 0xac, 0x4a, 0x25, 0xa4};
static const uint8_t ed_np32[]      = {0x1d, 0xb6, 0xc6, 0xf2, 0x6f, 0xe9, 0x18, 0x36, 0x14, 0xe7, 0x54, 0x38, 0xff, 0xa3, 0x6b, 0xea, 0xb1, 0xa2, 0x06, 0xf2, 0xfd, 0xba, 0x84, 0xff, 0xd2, 0xb5, 0x1d, 0xa3, 0x12, 0x54, 0x7e, 0x1b};
static const uint8_t ed_n32[]       = {0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0xde, 0xf9, 0xde, 0xa2, 0xf7, 0x9c, 0xd6, 0x58, 0x12, 0x63, 0x1a, 0x5c, 0xf5, 0xd3, 0xed};

// PKA_SW_CURVE_CURVE25519 (Montgomery)
static const uint8_t montg_m32[]       = {0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xed, };
static const uint8_t montg_a32[]       = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x6D, 0x06, };
static const uint8_t montg_b32[32]     = {0};
static const uint8_t montg_x32[]       = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, };
static const uint8_t montg_y32[]       = {0x20, 0xae, 0x19, 0xa1, 0xb8, 0xa0, 0x86, 0xb4, 0xe0, 0x1e, 0xdd, 0x2c, 0x77, 0x48, 0xd1, 0x4c, 0x92, 0x3d, 0x4d, 0x7e, 0x6d, 0x7c, 0x61, 0xb2, 0x29, 0xe9, 0xc5, 0xa2, 0x7e, 0xce, 0xd3, 0xd9, };
static const uint8_t montg_mp32[32]    = {0};
static const uint8_t montg_r_sqr32[32] = {0};
static const uint8_t montg_nr32[32]    = {0};
static const uint8_t montg_np32[32]    = {0};
static const uint8_t montg_n32[]       = {0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0xDE, 0xF9, 0xDE, 0xA2, 0xF7, 0x9C, 0xD6, 0x58, 0x12, 0x63, 0x1A, 0x5C, 0xF5, 0xD3, 0xED, };

ecc_set_type montg_ed_sets[] = {
// PKA_SW_CURVE_ED25519
{
   32,
   256,
   ed_m32,
   ed_a32,
   ed_d32,
   ed_x32,
   ed_y32,
   ed_mp32,
   ed_r_sqr32,
   ed_nr32,
   ed_np32,
   ed_n32
},
// PKA_SW_CURVE_CURVE25519
{
   32,
   256,
   montg_m32,
   montg_a32,
   montg_b32,
   montg_x32,
   montg_y32,
   montg_mp32,
   montg_r_sqr32,
   montg_nr32,
   montg_np32,
   montg_n32
}
};


// Montgomery
int pka_x25519(struct pka_state *state, enum eccCurve curve,
                uint8_t *k, uint8_t *u, uint8_t *b, uint8_t *c)
{
   int mod_size = pka_curve_size[curve];
   uint8_t *m = (uint8_t *)montg_ed_sets[curve - PKA_SW_CURVE_ED25519].m;
   int err = PKA_ERR;

   if (curve != PKA_SW_CURVE_CURVE25519)
   {
      PKA_REPORT_ERR("Invalid curve", curve);
      err = PKA_INVPARAM;
      return err;
   }

   b[0] &= 0x7F;
   err = pka_core_x25519_pmult(state, k, u, b, m, mod_size, c);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_x25519_pmult", err);
   }

   return err;
}


// Edwards
int pka_eddsa_base_pmult(struct pka_state *state, enum eccCurve curve, uint8_t *s, uint8_t *b, uint8_t *enc_point)
{
   int err = PKA_ERR;
   uint16_t mod_size = pka_curve_size[curve];
   struct ecc_point key;
   struct ecc_point curve_point;
   uint8_t *m = (uint8_t *)montg_ed_sets[curve - PKA_SW_CURVE_ED25519].m;
   uint8_t *d = (uint8_t *)montg_ed_sets[curve - PKA_SW_CURVE_ED25519].b;

   if (curve != PKA_SW_CURVE_ED25519)
   {
      PKA_REPORT_ERR("Invalid curve", curve);
      err = PKA_INVPARAM;
      return err;
   }

   curve_point.x = (uint8_t *)montg_ed_sets[curve - PKA_SW_CURVE_ED25519].x;
   curve_point.y = (uint8_t *)montg_ed_sets[curve - PKA_SW_CURVE_ED25519].y;

   // blinding modulo m (prime)
   err = pka_core_ecc_mod(state, b, m, b, mod_size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_mod", err);
      return err;
   }

   key.x = malloc(mod_size);
   key.y = enc_point;

   err = pka_core_eddsa_pmult(state, curve_point, s, b, m, d, mod_size, key);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_eddsa_pmult", err);
      free(key.x);
      return err;
   }

   enc_point[0] &= 0x7F;
   enc_point[0] |= (key.x[31] & 0x1) << 7;

   byte_reverse(enc_point, mod_size);
   free(key.x);

   return err;
}

int pka_eddsa_key_gen(struct pka_state *state, enum eccCurve curve,
                       uint8_t *privkey, uint8_t *b, uint8_t *keypub)
{
   uint8_t hash[SHA_512_SIZE] = {0};
   int err = PKA_ERR;

   err = pka_sha512(privkey, pka_curve_size[curve], hash);

   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512", err);
   }

   // lower 32 bytes of hash is used as scalar
   // prune scalar (lowest 3 bits cleared), (2nd highest bit of last byte set)
   // keypub output is encoded

   // prune scalar
   hash[0] = hash[0] & 0xF8;
   hash[31] = hash[31] & 0x7F;
   hash[31] = hash[31] | 0x40;

   b[pka_curve_size[curve]-1] &= 0x7F;
   byte_reverse(hash, pka_curve_size[curve]);

   err = pka_eddsa_base_pmult(state, curve, hash, b, keypub);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_eddsa_base_pmult", err);
   }

   return err;
}

int pka_eddsa_sig_gen_p0(struct pka_state *state, enum eccCurve curve, uint8_t *hash_r, uint8_t *b, uint8_t *sig_r)
{
   int err = PKA_ERR;
   uint16_t size = pka_curve_size[curve];
   uint8_t *hash_r_mod = NULL;
   uint8_t *n  = (uint8_t *)montg_ed_sets[curve - PKA_SW_CURVE_ED25519].n;

   byte_reverse(hash_r, 2*size);
   hash_r_mod = malloc(size);

   // hash_r modulo order
   err = pka_core_ecc_mod_dp(state, hash_r, n, hash_r_mod, size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_mod_dp", err);
      free(hash_r_mod);
      return err;
   }

   err = pka_eddsa_base_pmult(state, curve, hash_r_mod, b, sig_r);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_eddsa_base_pmult", err);
      free(hash_r_mod);
      return err;
   }

   byte_reverse(hash_r, 2*size);
   free(hash_r_mod);

   return err;
}

int pka_eddsa_sig_gen_p1(struct pka_state *state, enum eccCurve curve,
                         uint8_t *derived_k, uint8_t *hash_r, uint8_t *secret_scalar, uint8_t *sig_s)
{
   int err = PKA_ERR;
   uint16_t size = pka_curve_size[curve];
   uint8_t *derived_k_internal = NULL;

   uint8_t *n = (uint8_t *)montg_ed_sets[curve - PKA_SW_CURVE_ED25519].n;
   uint8_t *np = (uint8_t *)montg_ed_sets[curve - PKA_SW_CURVE_ED25519].np;
   uint8_t *nr = (uint8_t *)montg_ed_sets[curve - PKA_SW_CURVE_ED25519].nr;

   // leave derived_k the same on return
   derived_k_internal = malloc(SHA_512_SIZE);
   memcpy(derived_k_internal, derived_k, SHA_512_SIZE);

   // big endian
   byte_reverse(derived_k_internal, size*2);
   byte_reverse(hash_r, size*2);
   byte_reverse(secret_scalar, size);

   // derived_k_internal = derived_k_internal mod order
   err = pka_core_ecc_mod_dp(state, derived_k_internal, n, derived_k_internal, size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_mod_dp", err);
      goto EXIT_P1;
   }

   // secret_scalar = secret scalar mod order
   err = pka_core_ecc_mod(state, secret_scalar, n, secret_scalar, size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_mod", err);
      goto EXIT_P1;
   }

   // hash_r = hash_r mod order
   err = pka_core_ecc_mod_dp(state, hash_r, n, hash_r, size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_mod_dp", err);
      goto EXIT_P1;
   }

   // sig_s = (derived_k_internal * secret_scalar) mod order
   err = pka_core_eddsa_modmult_obp(state, derived_k_internal, secret_scalar, n, np, nr, size, sig_s);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_eddsa_modmult_obp", err);
      goto EXIT_P1;
   }

   // sig_s = (hash_r + sig_s) mod order
   err = pka_core_ecc_modadd(state, hash_r, (uint32_t)size, sig_s, n, sig_s, size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_modadd", err);
      goto EXIT_P1;
   }

   byte_reverse(sig_s, size);

EXIT_P1:
   free(derived_k_internal);
   return err;
}

int pka_eddsa_sig_ver(struct pka_state *state, enum eccCurve curve,
                      uint8_t *sig_r, uint8_t *sig_s, uint8_t *hash_k, uint8_t *keypub)
{
   int err = PKA_ERR;
   uint16_t size = pka_curve_size[curve];
   struct ecc_point keypub_point;
   struct ecc_point s_point;
   struct ecc_point hash_point;
   struct ecc_point recover_point;
   struct ecc_point curve_point;
   // no blinding input as a parameter
   uint8_t *b = NULL;
   uint8_t sign = 0;
   uint8_t *hash_k_internal = NULL;

   uint8_t *m = (uint8_t *)montg_ed_sets[curve - PKA_SW_CURVE_ED25519].m;
   uint8_t *d = (uint8_t *)montg_ed_sets[curve - PKA_SW_CURVE_ED25519].b;
   uint8_t *n = (uint8_t *)montg_ed_sets[curve - PKA_SW_CURVE_ED25519].n;
   curve_point.x = (uint8_t *)montg_ed_sets[curve - PKA_SW_CURVE_ED25519].x;
   curve_point.y = (uint8_t *)montg_ed_sets[curve - PKA_SW_CURVE_ED25519].y;

   // recover public keypub_point from encoded keypub
   keypub_point.x = malloc(size);
   keypub_point.y = malloc(size);
   memcpy(keypub_point.y, keypub, size);
   byte_reverse(keypub_point.y, size);
   // sign = MSB(keypub_point.y)
   sign = ((keypub_point.y[0] & 0x80) >> 7);
   // recover keypub_point.y
   keypub_point.y[0] &= 0x7F;
   // recover keypub_point.x

   err = pka_core_eddsa_xrecover(state, sign, keypub_point.y, m, d, size, keypub_point.x);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_eddsa_xrecover", err);
      goto EXIT_SIG_VER_PUB;
   }

   s_point.x = malloc(size);
   s_point.y = malloc(size);
   byte_reverse(sig_s, size);
   err = pka_core_eddsa_pmult(state, curve_point, sig_s, b, m, d, size, s_point);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_eddsa_pmult", err);
      goto EXIT_SIG_VER_S;
   }

   hash_point.x = malloc(size);
   hash_point.y = malloc(size);
   hash_k_internal = malloc(SHA_512_SIZE);
   memcpy(hash_k_internal, hash_k, SHA_512_SIZE);
   byte_reverse(hash_k_internal, SHA_512_SIZE);
   // hash_k = hash_k mod order
   err = pka_core_ecc_mod_dp(state, hash_k_internal, n, hash_k_internal, size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_mod_dp", err);
      goto EXIT_SIG_VER_HASH;
   }

   err = pka_core_eddsa_pmult(state, keypub_point, hash_k_internal, b, m, d, size, hash_point);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_eddsa_pmult", err);
      goto EXIT_SIG_VER_HASH;
   }

   recover_point.x = malloc(size);
   recover_point.y = malloc(size);
   memcpy(recover_point.y, sig_r, size);
   // sig_r
   byte_reverse(recover_point.y, size);
   // sign = MSB(sig_r)
   sign = ((recover_point.y[0] & 0x80) >> 7);
   // recover y
   recover_point.y[0] &= 0x7F;

   err = pka_core_eddsa_xrecover(state, sign, recover_point.y, m, d, size, recover_point.x);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_eddsa_xrecover", err);
      goto EXIT_SIG_VER;
   }

   // compare s_point vs (recover_point + hash_point)
   err = pka_core_eddsa_point_add(state, recover_point, hash_point, b, m, d, size, recover_point);
   if ((memcmp(s_point.x, recover_point.x, size) !=0) ||
       (memcmp(s_point.y, recover_point.y, size) !=0) )
   {
      err = PKA_VERFYFAIL;
      PKA_REPORT_ERR(" ", err);
   }

EXIT_SIG_VER:
   free(recover_point.x);
   free(recover_point.y);
EXIT_SIG_VER_HASH:
   free(hash_k_internal);
   free(hash_point.x);
   free(hash_point.y);
EXIT_SIG_VER_S:
   free(s_point.x);
   free(s_point.y);
EXIT_SIG_VER_PUB:
   free(keypub_point.x);
   free(keypub_point.y);

   return err;
}

int pka_eddsa_sign(struct pka_state *state, enum eccCurve curve,
                    uint8_t *keypriv, uint8_t *keypub,
                    uint8_t *b, uint8_t *msg, uint32_t msg_len,
                    uint8_t *sig_r, uint8_t *sig_s)
{
   sha512_state md;
   const uint32_t key_len = pka_curve_size[curve];
   const uint8_t sha512_size = SHA_512_SIZE;
   uint8_t hash_h[sha512_size];
   uint8_t hash_r[sha512_size];
   uint8_t hash_k[sha512_size];
   uint32_t len = sha512_size;

   int err = PKA_ERR;

   // hash_h = SHA-512(keypriv)
   if ((err = pka_sha512_init(&md)) != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_init", err);
   }

   if ((err = pka_sha512_process(keypriv, key_len, &md)) != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_process", err);
   }

   if ((err = pka_sha512_done(hash_h, &len, &md)) != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_done", err);
   }

   // secret scalar s = lower 32 bytes of h
   // prefix = upper 32 bytes of h
   // hash_r = SHA-512(prefix || msg)
   if ((err = pka_sha512_init(&md)) != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_init", err);
   }

   // prefix
   if ((err = pka_sha512_process(hash_h+key_len, key_len, &md)) != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_process", err);
   }

   // msg
   if ((err = pka_sha512_process(msg, msg_len, &md)) != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_process", err);
   }

   if ((err = pka_sha512_done(hash_r, &len, &md)) != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_done", err);
   }

   // build encoded pointR
   b[pka_curve_size[curve]-1] &= 0x7F;
   err = pka_eddsa_sig_gen_p0(state, curve, hash_r, b, sig_r);

   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_eddsa_sig_gen_p0", err);
   }

   // hash_k = SHA-512(sig_r || keypub || msg)
   if ((err = pka_sha512_init(&md)) != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_init", err);
   }

   if ((err = pka_sha512_process(sig_r, key_len, &md)) != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_process", err);
   }

   if ((err = pka_sha512_process(keypub, key_len, &md)) != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_process", err);
   }

   if ((err = pka_sha512_process(msg, msg_len, &md)) != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_process", err);
   }

   if ((err = pka_sha512_done(hash_k, &len, &md)) != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_done", err);
   }

   // prune scalar lower 32 bytes of hash_h
   hash_h[0]  = hash_h[0] & 0xF8;
   hash_h[31] = hash_h[31] & 0x7F;
   hash_h[31] = hash_h[31] | 0x40;

   err = pka_eddsa_sig_gen_p1(state, curve, hash_k, hash_r, hash_h, sig_s);

   if (err != PKA_OK)
   {
     PKA_REPORT_ERR("pka_eddsa_sig_gen_p1", err);
   }

   return err;
}
int pka_eddsa_verify(struct pka_state *state, enum eccCurve curve, uint8_t *sig_r, uint8_t *sig_s,
                        uint8_t *msg, uint32_t msg_len,uint8_t *keypub)
{
   sha512_state md;
   const uint32_t key_len = pka_curve_size[curve];
   const uint8_t sha512_size = SHA_512_SIZE;
   uint8_t hash_k[sha512_size];
   uint32_t len = sha512_size;
   int err = PKA_ERR;

   //add code here
   // hash_k = SHA-512(sig_r || keypub || msg)
   if ((err = pka_sha512_init(&md)) != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_init", err);
   }

   if ((err = pka_sha512_process(sig_r, key_len, &md)) != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_process", err);
   }

   if ((err = pka_sha512_process(keypub, key_len, &md)) != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_process", err);
   }

   if ((err = pka_sha512_process(msg, msg_len, &md)) != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_process", err);
   }

   if ((err = pka_sha512_done(hash_k, &len, &md)) != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_done", err);
   }

   err = pka_eddsa_sig_ver(state, curve, sig_r, sig_s, hash_k, keypub);

   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_eddsa_sig_ver", err);
   }

   return err;
}
