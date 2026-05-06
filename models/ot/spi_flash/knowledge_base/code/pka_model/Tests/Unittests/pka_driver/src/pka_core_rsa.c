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

#include <stdint.h>
#include "pka.h"
#include "pka_core_rsa.h"
#include "pka_driv.h"
#include "pka_hw.h"
#include "clp300_ram_fw.h"

uint16_t pka_core_rsa_reg_map(struct pka_state *state,
                              enum pka_op_reg_type reg, uint16_t size)
{
   uint8_t size_rsa_bld_cfg = 0;

   switch(size*8)
   {
   case 512:
      size_rsa_bld_cfg = RSA_512;
      break;
   case 768:   //partial radix case
   case 1024:
      size_rsa_bld_cfg = RSA_1024;
      break;
   case 1536:  //partial radix case
   case 2048:
      size_rsa_bld_cfg = RSA_2048;
      break;
   case 3072:  //partial radix case
   case 4096:
      size_rsa_bld_cfg = RSA_4096;
      break;
   default:
      PKA_REPORT_ERR("Invalid size", size);
      return PKA_INVRNGE;
   }
   if (size_rsa_bld_cfg <= (state->build_cfg & PKA_BC_RSA_SZ_MASK) >> PKA_BC_RSA_SZ)
   {
      uint16_t RSA_matrix[32][4] = {
               {0x400, 0x400, 0x400, 0x400},  // A0
               {0x440, 0x480, 0x500, 0x600},  // A1
               {0x480, 0x500, 0x600, 0},      // A2
               {0x4C0, 0x580, 0x700, 0},      // A3
               {0, 0, 0, 0},
               {0, 0, 0, 0},
               {0, 0, 0, 0},
               {0, 0, 0, 0},
               {0x800, 0x800, 0x800, 0x800}, // B0
               {0x840, 0x880, 0x900, 0xA00}, // B1
               {0x880, 0x900, 0xA00, 0},     // B2
               {0x8C0, 0x980, 0xB00, 0},     // B3
               {0, 0, 0, 0},
               {0, 0, 0, 0},
               {0, 0, 0, 0},
               {0, 0, 0, 0},
               {0xC00, 0xC00, 0xC00, 0xC00}, // C0
               {0xC40, 0xC80, 0xD00, 0xE00}, // C1
               {0xC80, 0xD00, 0xE00, 0},     // C2
               {0xCC0, 0xD80, 0xF00, 0},     // C3
               {0, 0, 0, 0},
               {0, 0, 0, 0},
               {0, 0, 0, 0},
               {0, 0, 0, 0},
               {0x1000, 0x1000, 0x1000, 0x1000}, // D0
               {0x1040, 0x1080, 0x1100, 0x1200}, // D1
               {0x1080, 0x1100, 0x1200, 0x1400}, // D2
               {0x10C0, 0x1180, 0x1300, 0x1600}, // D3
               {0x1100, 0x1200, 0x1400, 0},      // D4
               {0x1140, 0x1280, 0x1500, 0},      // D5
               {0x1180, 0x1300, 0x1600, 0},      // D6
               {0x11C0, 0x1380, 0x1700, 0}       // D7
      };
      if (RSA_matrix[reg][size_rsa_bld_cfg - 1] == 0)
      {
         PKA_REPORT_ERR2("Invalid reg or size", reg, size_rsa_bld_cfg);
         return 0;
      }
      else
      {
         // success
         return RSA_matrix[reg][size_rsa_bld_cfg - 1];
      }
   }
   else
   {
      PKA_REPORT_ERR("Invalid size", size_rsa_bld_cfg);
   };
   return 0;
}

int pka_core_rsa_precomp(struct pka_state *state, struct pka_rsa_mod *n, uint16_t mlen)
{
   int err = PKA_ERR;

   if (pka_driv_lock() != PKA_OK)
   {
      PKA_REPORT("lock fail");
      err = PKA_LOCKFAIL;
      return err;
   }

   // D0 with modulus
   pka_driv_write_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, D0, mlen), n->m, mlen );

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);
   err = pka_core_go(state, ELP_CLUE_ENTRY_CALC_R_INV, mlen);
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

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);
   err = pka_core_go(state, ELP_CLUE_ENTRY_CALC_MP, mlen);
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

   // Read D1 into mp buffer
   pka_driv_read_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, D1, mlen), n->mp, mlen);

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);
   err = pka_core_go(state, ELP_CLUE_ENTRY_CALC_R_SQR, mlen);
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

   // Read D3 into r_sqr buffer
   pka_driv_read_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, D3, mlen), n->r_sqr, mlen);

DONE:
   if (pka_driv_unlock() != PKA_OK)
   {
      PKA_REPORT("unlock fail");
   }

   return err;
}

int pka_core_rsa_modexp(struct pka_state *state, struct pka_rsa_key *key, uint8_t *base, uint8_t  *result)
{
   int err = PKA_ERR;

   if (pka_driv_lock() != PKA_OK)
   {
      PKA_REPORT("lock fail");
      err = PKA_LOCKFAIL;
      return err;
   }

   // write A0 with base x
   pka_driv_write_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, A0, key->mod_size), base, key->mod_size);
   // write D2 with exp y
   pka_driv_write_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, D2, key->mod_size), key->exp, key->mod_size);
   // D0 with n.m
   pka_driv_write_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, D0, key->mod_size), key->n.m, key->mod_size);
   // D1 with n.mp
   pka_driv_write_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, D1, key->mod_size), key->n.mp, key->mod_size);
   // D3 with n.r_sqr
   pka_driv_write_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, D3, key->mod_size), key->n.r_sqr, key->mod_size);

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);
   err = pka_core_go(state, ELP_CLUE_ENTRY_MODEXP, key->mod_size);
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

   // write from A0 (c=x^y) to result buffer
   pka_driv_read_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, A0, key->mod_size), result, key->mod_size);

DONE:
   if (pka_driv_unlock() != PKA_OK)
   {
      PKA_REPORT("unlock fail");
   }

   return err;
}

int pka_core_rsa_modexp_crt(struct pka_state *state, struct pka_rsa_crt_key *crtkey, uint8_t *base, uint8_t  *result)
{
   int err = PKA_ERR;

   if (pka_driv_lock() != PKA_OK)
   {
      PKA_REPORT("lock fail");
      err = PKA_LOCKFAIL;
      return err;
   }

   // Write A2 with low half of base (xlow)
   pka_driv_write_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, A2, crtkey->mod_size/2), base + (crtkey->mod_size/2), crtkey->mod_size/2);
   // Write A3 with high half of base (xhigh)
   pka_driv_write_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, A3, crtkey->mod_size/2), base, crtkey->mod_size/2);
   // Write B2 with p.m
   pka_driv_write_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, B2, crtkey->mod_size/2), crtkey->p.m, crtkey->mod_size/2);
   // Write B3 with q.m
   pka_driv_write_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, B3, crtkey->mod_size/2), crtkey->q.m, crtkey->mod_size/2);
   // Write C2 with qInv
   pka_driv_write_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, C2, crtkey->mod_size/2), crtkey->qinv, crtkey->mod_size/2);
   // Write C3 with dP
   pka_driv_write_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, C3, crtkey->mod_size/2), crtkey->dp, crtkey->mod_size/2);
   // Write D2 with dq
   pka_driv_write_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, D2, crtkey->mod_size/2), crtkey->dq, crtkey->mod_size/2);
   // Write D5 with p.r_sqr
   pka_driv_write_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, D5, crtkey->mod_size/2), crtkey->p.r_sqr, crtkey->mod_size/2);
   // Write D4 with p.mp
   pka_driv_write_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, D4, crtkey->mod_size/2), crtkey->p.mp, crtkey->mod_size/2);
   // Write D3 with q.r_sqr
   pka_driv_write_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, D3, crtkey->mod_size/2), crtkey->q.r_sqr, crtkey->mod_size/2);
   // Write D6 with q.mp
   pka_driv_write_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, D6, crtkey->mod_size/2), crtkey->q.mp, crtkey->mod_size/2);

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);
   err = pka_core_go(state, ELP_CLUE_ENTRY_CRT, crtkey->mod_size/2);
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

   // copy A0 and A1 (two halves of result c) into the provided c buffer
   pka_driv_read_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, A0, crtkey->mod_size/2), result + (crtkey->mod_size/2), (crtkey->mod_size/2));
   pka_driv_read_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, A1, crtkey->mod_size/2), result, (crtkey->mod_size/2));

DONE:
   if (pka_driv_unlock() != PKA_OK)
   {
      PKA_REPORT("unlock fail");
   }

   return err;
}

int pka_core_rsa_mul(struct pka_state *state, uint8_t *a, uint8_t *b, uint16_t mlen, uint8_t *c)
{
   int err = PKA_ERR;

   if (pka_driv_lock() != PKA_OK)
   {
      PKA_REPORT("lock fail");
      err = PKA_LOCKFAIL;
      return err;
   }

   // ELP_CLUE_ENTRY_MULT: a * b = c
   // A0 with a
   pka_driv_write_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, A0, mlen), a, mlen);
   // B0 with b
   pka_driv_write_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, B0, mlen), b, mlen);

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);
   err = pka_core_go(state, ELP_CLUE_ENTRY_MULT, mlen);
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

   // Read result from C0 and C1 in c
   pka_driv_read_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, C0, mlen),
                        c + mlen, mlen);
   pka_driv_read_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, C1, mlen),
                        c, mlen);

DONE:
   if (pka_driv_unlock() != PKA_OK)
   {
      PKA_REPORT("unlock fail");
   }

   return err;
}

int pka_core_rsa_keysetup_crt(struct pka_state *state, struct pka_rsa_crt_key *pka_key,
                              uint8_t *d, uint16_t dlen)
{
   int err = PKA_ERR;

   if (pka_driv_lock() != PKA_OK)
   {
      PKA_REPORT("lock fail");
      err = PKA_LOCKFAIL;
      return err;
   }

   // ELP_CLUE_ENTRY_CRT_KEY_SETUP: p, q, d = dP, dQ, qP
   // D0 with p
   pka_driv_write_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, D0, pka_key->mod_size / 2),
                         pka_key->p.m, pka_key->mod_size / 2);
   // B1 with q
   pka_driv_write_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, B1, pka_key->mod_size / 2),
                         pka_key->q.m, pka_key->mod_size / 2);
   // D1 and D3 with d
   pka_driv_write_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, D1, pka_key->mod_size / 2),
                         d + (dlen - (pka_key->mod_size / 2)), pka_key->mod_size / 2);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, D3, pka_key->mod_size / 2),
                         d, dlen - (pka_key->mod_size / 2));

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);
   err = pka_core_go(state, ELP_CLUE_ENTRY_CRT_KEY_SETUP, pka_key->mod_size / 2);
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

   // Read result from B1 in dP
   pka_driv_read_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, B1, pka_key->mod_size / 2),
                        pka_key->dp, pka_key->mod_size / 2);
   // Read result from C0 in dQ
   pka_driv_read_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, C0, pka_key->mod_size / 2),
                        pka_key->dq, pka_key->mod_size / 2);
   // Read result from A3 in qP
   pka_driv_read_bigint((uint32_t)state->base+pka_core_rsa_reg_map(state, A3, pka_key->mod_size / 2),
                        pka_key->qinv, pka_key->mod_size / 2);

DONE:
   if (pka_driv_unlock() != PKA_OK)
   {
      PKA_REPORT("unlock fail");
   }

   return err;
}

