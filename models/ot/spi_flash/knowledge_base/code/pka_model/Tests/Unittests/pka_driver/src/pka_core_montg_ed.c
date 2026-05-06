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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pka_core.h"
#include "pka_core_ecc.h"
#include "pka_driv.h"
#include "pka_hw.h"
#include "sha512.h"


int pka_core_x25519_pmult(struct pka_state *state, uint8_t *k, uint8_t *u,
                          uint8_t *b, const uint8_t *m, int mod_size, uint8_t *c)
{
   int err = PKA_ERR;
   uint8_t k_param[32] = {0};

   // k_param = 121666
   k_param[29] = 0x01;
   k_param[30] = 0xDB;
   k_param[31] = 0x42;

   if (pka_driv_lock() != PKA_OK)
   {
      PKA_REPORT("lock fail");
      err = PKA_LOCKFAIL;
      return err;
   }

   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A2, mod_size), u, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A7, mod_size), b, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D0, mod_size), m, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D2, mod_size), k_param, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D7, mod_size), k, mod_size);

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, ECC_ENABLE_BLINDING << PKA_FLAG_F0);

   // GO
   err = pka_core_go(state, ELP_CLUE_ENTRY_C25519_PMULT, mod_size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("Entry Point Error", err);
      goto DONE;
   }
   err = pka_core_wait(state);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("Hardware Detected Error", err);
      goto DONE;
   }

   // result
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A2, mod_size), c, mod_size);

DONE:
   if (pka_driv_unlock() != PKA_OK)
   {
      PKA_REPORT("unlock fail");
   }

   return err;
}

/*
 * Not used and provided as a reference.
 *
int pka_core_montg_ed_modmult(struct pka_state *state, uint8_t *x, uint8_t *y, const uint8_t *m,
                              int mod_size, uint8_t *result)
{
   int err = PKA_ERR;

   if (pka_driv_lock() != PKA_OK)
   {
      PKA_REPORT("lock fail");
      err = PKA_LOCKFAIL;
      return err;
   }

   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A0, mod_size), x, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, B0, mod_size), y, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D0, mod_size), m, mod_size);

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);

   // GO
   err = pka_core_go(state, ELP_CLUE_ENTRY_CED25519_MODMULT, mod_size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("Entry Point Error", err);
      goto DONE;
   }
   err = pka_core_wait(state);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("Hardware Detected Error", err);
      goto DONE;
   }

   // result
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A0, mod_size), result, mod_size);

DONE:
   if (pka_driv_unlock() != PKA_OK)
   {
      PKA_REPORT("unlock fail");
   }

   return err;
}
 */


int pka_core_eddsa_pmult(struct pka_state *state, struct ecc_point point, uint8_t *s, uint8_t *b,
                         const uint8_t *m, const uint8_t *d, int mod_size, struct ecc_point enc_point)
{
   int err = PKA_ERR;

   if (pka_driv_lock() != PKA_OK)
   {
      PKA_REPORT("lock fail");
      err = PKA_LOCKFAIL;
      return err;
   }

   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A2, mod_size), point.x, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, B2, mod_size), point.y, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, C5, mod_size), d, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D0, mod_size), m, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D7, mod_size), s, mod_size);

   if (b != NULL)
   {
      pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A7, mod_size), b, mod_size);
      pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, ECC_ENABLE_BLINDING << PKA_FLAG_F0);
   }
   else
   {
      pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);
   }

   // GO
   err = pka_core_go(state, ELP_CLUE_ENTRY_ED25519_PMULT, mod_size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("Entry Point Error", err);
      goto DONE;
   }
   err = pka_core_wait(state);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("Hardware Detected Error", err);
      goto DONE;
   }

   // result
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A2, mod_size), enc_point.x, mod_size);
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, B2, mod_size), enc_point.y, mod_size);

DONE:
   if (pka_driv_unlock() != PKA_OK)
   {
      PKA_REPORT("unlock fail");
   }

   return err;
}


int pka_core_eddsa_modmult_obp(struct pka_state *state, uint8_t *x, uint8_t *y, const uint8_t *m,
                               const uint8_t *mp, const uint8_t *r_sqr, int mod_size, uint8_t *result)
{
   int err = PKA_ERR;

   if (pka_driv_lock() != PKA_OK)
   {
      PKA_REPORT("lock fail");
      err = PKA_LOCKFAIL;
      return err;
   }

   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A0, mod_size), x, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, B0, mod_size), y, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D0, mod_size), m, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D1, mod_size), mp, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D3, mod_size), r_sqr, mod_size);

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);

   // GO
   err = pka_core_go(state, ELP_CLUE_ENTRY_ED25519_OBP_MONTMULT, mod_size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("Entry Point Error", err);
      goto DONE;
   }
   err = pka_core_wait(state);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("Hardware Detected Error", err);
      goto DONE;
   }

   // result
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A0, mod_size), result, mod_size);

DONE:
   if (pka_driv_unlock() != PKA_OK)
   {
      PKA_REPORT("unlock fail");
   }

   return err;
}

int pka_core_eddsa_xrecover(struct pka_state *state, uint8_t sign, uint8_t *y, const uint8_t *m,
                            const uint8_t *d, int mod_size, uint8_t *result_x)
{
   // only for 32 byte (PKA_CURVE_ED25519_BYTE) size now
   int err = PKA_ERR;
   uint8_t x[32] = {0};
   uint8_t Pp3d8[32] =
   { 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, };
   uint8_t I[32] =
   { 0x2B, 0x83, 0x24, 0x80, 0x4F, 0xC1, 0xDF, 0x0B,
     0x2B, 0x4D, 0x00, 0x99, 0x3D, 0xFB, 0xD7, 0xA7,
     0x2F, 0x43, 0x18, 0x06, 0xAD, 0x2F, 0xE4, 0x78,
     0xC4, 0xEE, 0x1B, 0x27, 0x4A, 0x0E, 0xA0, 0xB0, };

   if (mod_size != PKA_CURVE_ED25519_BYTE)
   {
      PKA_REPORT_ERR("Unsupported Edwards curve size", mod_size);
      return PKA_INVPARAM;
   }

   x[mod_size-1] = sign;

   if (pka_driv_lock() != PKA_OK)
   {
      PKA_REPORT("lock fail");
      err = PKA_LOCKFAIL;
      return err;
   }

   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, B2, mod_size), y, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, B5, mod_size), x, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, C5, mod_size), d, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, C6, mod_size), Pp3d8, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, C7, mod_size), I, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D0, mod_size), m, mod_size);

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);

   // GO
   err = pka_core_go(state, ELP_CLUE_ENTRY_ED25519_XRECOVER, mod_size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("Entry Point Error", err);
      goto DONE;
   }
   err = pka_core_wait(state);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("Hardware Detected Error", err);
      goto DONE;
   }

   // result
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A2, mod_size), result_x, mod_size);

DONE:
   if (pka_driv_unlock() != PKA_OK)
   {
      PKA_REPORT("unlock fail");
   }

   return err;
}

int pka_core_eddsa_point_add(struct pka_state *state, struct ecc_point p, struct ecc_point q, uint8_t *b,
                             const uint8_t *m, const uint8_t *d, int mod_size, struct ecc_point result)
{
   int err = PKA_ERR;

   if (pka_driv_lock() != PKA_OK)
   {
      PKA_REPORT("lock fail");
      err = PKA_LOCKFAIL;
      return err;
   }

   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A2, mod_size), p.x, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, B2, mod_size), p.y, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A3, mod_size), q.x, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, B3, mod_size), q.y, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, C5, mod_size), d, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D0, mod_size), m, mod_size);

   if (b != NULL)
   {
      pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A7, mod_size), b, mod_size);
      pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, ECC_ENABLE_BLINDING << PKA_FLAG_F0);
   }
   else
   {
      pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);
   }

   // GO
   err = pka_core_go(state, ELP_CLUE_ENTRY_ED25519_PADD, mod_size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("Entry Point Error", err);
      goto DONE;
   }
   err = pka_core_wait(state);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("Hardware Detected Error", err);
      goto DONE;
   }

   // result
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A2, mod_size), result.x, mod_size);
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, B2, mod_size), result.y, mod_size);

DONE:
   if (pka_driv_unlock() != PKA_OK)
   {
      PKA_REPORT("unlock fail");
   }

   return err;
}
