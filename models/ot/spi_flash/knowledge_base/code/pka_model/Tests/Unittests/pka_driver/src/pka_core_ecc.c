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
#include <string.h>
#include <stdlib.h>
#include "pka_defines.h"
#include "pka.h"
#include "pka_core_ecc.h"
#include "pka_driv.h"

// re-order the bytes around
void byte_reverse(uint8_t *s, int len)
{
  int     ix = 0;
  int     iy = len - 1;
  uint8_t t = 0;

  while (ix < iy) {
    t     = s[ix];
    s[ix] = s[iy];
    s[iy] = t;
    ++ix;
    --iy;
  }
}

uint16_t pka_core_ecc_reg_map(struct pka_state *state,
                              enum pka_op_reg_type reg, uint16_t size)
{
   uint8_t size_ecc_bld_cfg = 0;

   switch(size*8)
   {
   // 32 bytes
   case 256:
      size_ecc_bld_cfg = ECC_256;
      break;
   // 48 bytes
   case 384:
      size_ecc_bld_cfg = ECC_512;
      break;
   // 64 bytes
   case 512:
      size_ecc_bld_cfg = ECC_512;
      break;
   // 66 bytes
   case 528:// NIST521
      size_ecc_bld_cfg = ECC_1024;
      break;
   // 128 bytes
   case 1024:
      size_ecc_bld_cfg = ECC_1024;
      break;
   default:
      PKA_REPORT_ERR("Invalid size", size);
      return PKA_INVRNGE;
   }
   if (size_ecc_bld_cfg <= (state->build_cfg & PKA_BC_ECC_SZ_MASK) >> PKA_BC_ECC_SZ)
   {
      uint16_t ECC_matrix[32][3] = {
               {0x400, 0x400, 0x400},    // A0
               {0x420, 0x440, 0x480},    // A1
               {0x440, 0x480, 0x500},    // A2
               {0x460, 0x4C0, 0x580},    // A3
               {0x480, 0x500, 0x600},    // A4
               {0x4A0, 0x540, 0x680},    // A5
               {0x4C0, 0x580, 0x700},    // A6
               {0x4E0, 0x5C0, 0x780},    // A7
               {0x800, 0x800, 0x800},    // B0
               {0x820, 0x840, 0x880},    // B1
               {0x840, 0x880, 0x900},    // B2
               {0x860, 0x8C0, 0x980},    // B3
               {0x880, 0x900, 0xA00},    // B4
               {0x8A0, 0x940, 0xA80},    // B5
               {0x8C0, 0x980, 0xB00},    // B6
               {0x8E0, 0x9C0, 0xB80},    // B7
               {0xC00, 0xC00, 0xC00},    // C0
               {0xC20, 0xC40, 0xC80},    // C1
               {0xC40, 0xC80, 0xD00},    // C2
               {0xC60, 0xCC0, 0xD80},    // C3
               {0xC80, 0xD00, 0xE00},    // C4
               {0xCA0, 0xD40, 0xE80},    // C5
               {0xCC0, 0xD80, 0xF00},    // C6
               {0xCE0, 0xDC0, 0xF80},    // C7
               {0x1000, 0x1000, 0x1000}, // D0
               {0x1020, 0x1040, 0x1080}, // D1
               {0x1040, 0x1080, 0x1100}, // D2
               {0x1060, 0x10C0, 0x1180}, // D3
               {0x1080, 0x1100, 0x1200}, // D4
               {0x10A0, 0x1140, 0x1280}, // D5
               {0x10C0, 0x1180, 0x1300}, // D6
               {0x10E0, 0x11C0, 0x1380}  // D7
      };
      if (ECC_matrix[reg][size_ecc_bld_cfg - 1] == 0)
      {
         PKA_REPORT_ERR2("Invalid reg or size", reg, size_ecc_bld_cfg);
         return PKA_INVPARAM;
      }
      else
      {
         // success
         return ECC_matrix[reg][size_ecc_bld_cfg - 1];
      }
   }
   else
   {
      PKA_REPORT_ERR("Invalid size", size_ecc_bld_cfg);
   };
   return PKA_INVPARAM;
}

/*
 * This function performs the precompute with modulus (m) as input and generates the Montgomery
 * modulus prime (mp) and Montgomery r**2 (r_sqr) modulus m as output.  It also used to perform the
 * precompute with the order of the curve (n) as input and generates the inverse of n mod r (np)
 * and r**2 (nr) as output.
 * It was used to compute the ecc_sets array parameters mp, r_sqr and np, nr in this file.
 * This is included as comments so that it can be used for other curves. Note that it cannot be used
 * for the Edwards curves.
 *
int pka_core_ecc_precomp(struct pka_state *state, uint8_t *m, uint16_t mlen, uint8_t *mp, uint8_t *r_sqr)
{
   int err = PKA_ERR;

   if (pka_driv_lock() != PKA_OK)
   {
      PKA_REPORT("lock fail");
      err = PKA_LOCKFAIL;
      return err;
   }

   // D0 with modulus
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D0, mlen), m, mlen );

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
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D1, mlen), mp, mlen);

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
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D3, mlen), r_sqr, mlen);

DONE:
   if (pka_driv_unlock() != PKA_OK)
   {
      PKA_REPORT("unlock fail");
   }

   return err;
}
 */

int pka_core_ecc_point_validate(struct pka_state *state,
                                const uint8_t *m, const uint8_t *a, const uint8_t *b,
                                const uint8_t *mp, const uint8_t *r_sqr,
                                int mod_size, struct ecc_point point)
{
   int err = PKA_ERR;

   if (mod_size == PKA_CURVE_NIST_P521_BYTE)
   {
      PKA_REPORT_ERR("Invalid mod_size", mod_size);
      err = PKA_INVRNGE;
      return err;
   }

   if (pka_driv_lock() != PKA_OK)
   {
      PKA_REPORT("lock fail");
      err = PKA_LOCKFAIL;
      return err;
   }

   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A2, mod_size), point.x, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, B2, mod_size), point.y, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A6, mod_size), a, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A7, mod_size), b, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D0, mod_size), m, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D1, mod_size), mp, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D3, mod_size), r_sqr, mod_size);

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);

   // GO
   err = pka_core_go(state, ELP_CLUE_ENTRY_PVER, mod_size);
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

   if ((pka_driv_read((uint32_t)state->base + PKA_REG_FLAGS)& (1 << PKA_FLAG_ZERO)) != 1 )
   {
      err = PKA_PVER_ERR;
   }
   else
   {
      err = PKA_OK;
   }

DONE:
   if (pka_driv_unlock() != PKA_OK)
   {
      PKA_REPORT("unlock fail");
   }

   return err;
}

int pka_core_ecc_point_validate_521(struct pka_state *state,
                                    const uint8_t *m, const uint8_t *a, const uint8_t *b,
                                    int mod_size, struct ecc_point point)
{
   uint8_t tmp[80]={0};
   int err = PKA_ERR;

   if (mod_size != PKA_CURVE_NIST_P521_BYTE)
   {
      PKA_REPORT_ERR("Invalid mod_size", mod_size);
      err = PKA_INVRNGE;
      return err;
   }

   if (pka_driv_lock() != PKA_OK)
   {
      PKA_REPORT("lock fail");
      err = PKA_LOCKFAIL;
      return err;
   }

   //0-extend all registers dictated by the CTRL_PARTIAL_RADIX
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A2, mod_size), tmp, 80);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, B2, mod_size), tmp, 80);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A6, mod_size), tmp, 80);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A7, mod_size), tmp, 80);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D0, mod_size), tmp, 80);

   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A2, mod_size), point.x, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, B2, mod_size), point.y, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A6, mod_size), a, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A7, mod_size), b, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D0, mod_size), m, mod_size);

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);

   // GO
   err = pka_core_go(state, ELP_CLUE_ENTRY_PVER_521, mod_size);
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

   if ((pka_driv_read((uint32_t)state->base + PKA_REG_FLAGS)& (1 << PKA_FLAG_ZERO)) != 1 )
   {
      err = PKA_PVER_ERR;
   }
   else
   {
      err = PKA_OK;
   }

DONE:
   if (pka_driv_unlock() != PKA_OK)
   {
      PKA_REPORT("unlock fail");
   }

   return err;
}

int pka_core_ecc_pmult(struct pka_state *state, struct ecc_point keypub, const uint8_t *keypriv, const uint8_t *b,
                       const uint8_t *m, const uint8_t *a, const uint8_t *mp, const uint8_t *r_sqr,
                       int mod_size, struct ecc_point secret)
{
   int err = PKA_ERR;

   if (pka_driv_lock() != PKA_OK)
   {
      PKA_REPORT("lock fail");
      err = PKA_LOCKFAIL;
      return err;
   }

   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A2, mod_size), keypub.x, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, B2, mod_size), keypub.y, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A6, mod_size), a, mod_size);

   //for a 384-bit curve, the Key must be 0 extended to 512 bits and the full 512-bit Key must be written to register D7
   if (mod_size == PKA_CURVE_NIST_P384_BYTE)
   {
      uint8_t keypriv_ext[64];
      memset(keypriv_ext, 0, 16);
      memcpy(keypriv_ext + 16, keypriv, mod_size);
      pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D7, mod_size), keypriv_ext, 64);
   }
   // for a 521-bit curve you better use pmult_521 function instead
   else if (mod_size >= PKA_CURVE_NIST_P521_BYTE)
   {
      PKA_REPORT_ERR("Invalid mod_size", mod_size);
      err = PKA_INVRNGE;
      goto DONE;
   }
   else
   {
      pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D7, mod_size), keypriv, mod_size);
   }
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A7, mod_size), b, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D0, mod_size), m, mod_size);

   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D1, mod_size), mp, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D3, mod_size), r_sqr, mod_size);

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, ECC_ENABLE_BLINDING << PKA_FLAG_F0);

   // GO
   err = pka_core_go(state, ELP_CLUE_ENTRY_PMULT, mod_size);
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

   // result secret
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A2, mod_size), secret.x, mod_size);
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, B2, mod_size), secret.y, mod_size);

DONE:
   if (pka_driv_unlock() != PKA_OK)
   {
      PKA_REPORT("unlock fail");
   }

   return err;
}

int pka_core_ecc_pmult_521(struct pka_state *state, struct ecc_point keypub, const uint8_t *keypriv, const uint8_t *b,
                           const uint8_t *m, const uint8_t *a, int mod_size, struct ecc_point secret)
{
   int err = PKA_ERR;

   if (pka_driv_lock() != PKA_OK)
   {
      PKA_REPORT("lock fail");
      err = PKA_LOCKFAIL;
      return err;
   }

   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A2, mod_size), keypub.x, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, B2, mod_size), keypub.y, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A6, mod_size), a, mod_size);

   // for a curve other than 521-bit you better use pmult function instead
   if (mod_size != PKA_CURVE_NIST_P521_BYTE)
   {
      PKA_REPORT_ERR("Invalid mod_size", mod_size);
      err = PKA_INVRNGE;
      goto DONE;
   }
   else
   {
      uint8_t keypriv_ext[128];
      memset(keypriv_ext, 0, 62);
      memcpy(keypriv_ext + 62, keypriv, mod_size);
      pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D7, mod_size), keypriv_ext, 128);
   }

   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A7, mod_size), b, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D0, mod_size), m, mod_size);

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, ECC_ENABLE_BLINDING << PKA_FLAG_F0);

   // GO
   err = pka_core_go(state, ELP_CLUE_ENTRY_PMULT_521, mod_size);
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

   // result secret
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A2, mod_size), secret.x, mod_size);
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, B2, mod_size), secret.y, mod_size);

DONE:
   if (pka_driv_unlock() != PKA_OK)
   {
      PKA_REPORT("unlock fail");
   }

   return err;
}

// Shamir's Trick
// result = (p * key_p) + (q * key_q)
int pka_core_pmult2add(struct pka_state *state, struct ecc_point p, const uint8_t *key_p, struct ecc_point q, const uint8_t *key_q,
                       const uint8_t *m, const uint8_t *a, const uint8_t *mp, const uint8_t *r_sqr,
                       int mod_size, struct ecc_point result)
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
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A6, mod_size), a, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D0, mod_size), m, mod_size);

   // for a 384-bit curve, the Key must be 0 extended to 512 bits and the full 512-bit Key must be written to register D7
   if (mod_size == PKA_CURVE_NIST_P384_BYTE)
   {
      uint8_t keypriv_ext[64];
      memset(keypriv_ext, 0, 16);
      memcpy(keypriv_ext + 16, key_p, mod_size);
      pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A7, mod_size), keypriv_ext, 64);
      memcpy(keypriv_ext + 16, key_q, mod_size);
      pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D7, mod_size), keypriv_ext, 64);
   }
   // for a 521-bit curve you better use pmult2add_521 function instead
   else if (mod_size >= PKA_CURVE_NIST_P521_BYTE)
   {
      PKA_REPORT_ERR("Invalid mod_size", mod_size);
      err = PKA_INVRNGE;
      goto DONE;
   }
   else
   {
      pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A7, mod_size), key_p, mod_size);
      pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D7, mod_size), key_q, mod_size);
   }

   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D1, mod_size), mp, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D3, mod_size), r_sqr, mod_size);

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);

   // GO
   err = pka_core_go(state, ELP_CLUE_ENTRY_SHAMIR, mod_size);
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

   // result result
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A2, mod_size), result.x, mod_size);
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, B2, mod_size), result.y, mod_size);

DONE:
   if (pka_driv_unlock() != PKA_OK)
   {
      PKA_REPORT("unlock fail");
   }

   return err;
}

// Shamir's Trick 521
// result = (p * key_p) + (q * key_q)
int pka_core_pmult2add_521(struct pka_state *state, struct ecc_point p, const uint8_t *key_p, struct ecc_point q, const uint8_t *key_q,
                           const uint8_t *m, const uint8_t *a, int mod_size, struct ecc_point result)
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
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A6, mod_size), a, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D0, mod_size), m, mod_size);

   // for a curve other than 521-bit you better use pmult2add function instead
   if (mod_size != PKA_CURVE_NIST_P521_BYTE)
   {
      PKA_REPORT_ERR("Invalid mod_size", mod_size);
      err = PKA_INVRNGE;
      goto DONE;
   }
   else
   {
      uint8_t keypriv_ext[128];
      memset(keypriv_ext, 0, 62);
      memcpy(keypriv_ext + 62, key_p, mod_size);
      pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A7, mod_size), keypriv_ext, 128);
      memcpy(keypriv_ext + 62, key_q, mod_size);
      pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D7, mod_size), keypriv_ext, 128);
   }

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);

   // GO
   err = pka_core_go(state, ELP_CLUE_ENTRY_SHAMIR_521, mod_size);
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

   // result result
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A2, mod_size), result.x, mod_size);
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, B2, mod_size), result.y, mod_size);

DONE:
   if (pka_driv_unlock() != PKA_OK)
   {
      PKA_REPORT("unlock fail");
   }

   return err;
}

int pka_core_ecc_point_add(struct pka_state *state, struct ecc_point p, struct ecc_point q,
                           const uint8_t *m, const uint8_t *a, const uint8_t *mp, const uint8_t *r_sqr,
                           int mod_size, struct ecc_point result)
{
   int err = PKA_ERR;

   if (mod_size == PKA_CURVE_NIST_P521_BYTE)
   {
      PKA_REPORT_ERR("Invalid mod_size", mod_size);
      err = PKA_INVRNGE;
      return err;
   }

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

   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A6, mod_size), a, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D0, mod_size), m, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D1, mod_size), mp, mod_size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D3, mod_size), r_sqr, mod_size);

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);

   // GO
   err = pka_core_go(state, ELP_CLUE_ENTRY_PADD, mod_size);
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

int pka_core_ecc_point_add_521(struct pka_state *state, struct ecc_point p, struct ecc_point q,
                               const uint8_t *m, const uint8_t *a,
                               int mod_size, struct ecc_point result)
{
   int err = PKA_ERR;

   (void)m;
   (void)a;

   if (mod_size != PKA_CURVE_NIST_P521_BYTE)
   {
      PKA_REPORT_ERR("Invalid mod_size", mod_size);
      err = PKA_INVRNGE;
      return err;
   }

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

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);

   // GO
   err = pka_core_go(state, ELP_CLUE_ENTRY_PADD_521, mod_size);
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

int pka_core_ecc_modmult(struct pka_state *state, uint8_t *x, uint8_t *y, uint8_t *m, uint8_t *mp, uint8_t *r_sqr, uint8_t *result, uint16_t size)
{
   uint32_t entry_pnt = 0;
   int err = PKA_ERR;
   uint8_t tmp[80]={0};

   if (pka_driv_lock() != PKA_OK)
   {
      PKA_REPORT("lock fail");
      err = PKA_LOCKFAIL;
      return err;
   }

   //TODO 0-extend to the size needed only as following
   /*It is mandatory in this mode to 0-extend all registers out to the size dictated by the CTRL_PARTIAL_RADIX
   field. Depending on the configured ALU width this would be 544 bit for a 32-bit ALU, 576 bits for a 64-bit
   ALU, and 640 bits for a 128-bit ALU.*/

   if(size == PKA_CURVE_NIST_P521_BYTE)
   {
      pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A0, size), tmp, 80);
      pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, B0, size), tmp, 80);
      pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D0, size), tmp, 80);
      pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D1, size), tmp, 80);
      pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D3, size), tmp, 80);
   }

   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A0, size), x, size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, B0, size), y, size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D0, size), m, size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D1, size), mp, size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D3, size), r_sqr, size);

   if (size == PKA_CURVE_NIST_P521_BYTE)
   {
      entry_pnt = ELP_CLUE_ENTRY_M_521_MONTMULT;
   }
   else
   {
      entry_pnt = ELP_CLUE_ENTRY_MODMULT;
   }

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);

   // GO
   err = pka_core_go(state, entry_pnt, size);
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
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A0, size), result, size);

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
int pka_core_ecc_modinv(struct pka_state *state, uint8_t *x, uint8_t *m, uint8_t *result, uint16_t size)
{
   uint32_t entry_pnt = 0;
   int err = PKA_ERR;

   if (pka_driv_lock() != PKA_OK)
   {
      PKA_REPORT("lock fail");
      err = PKA_LOCKFAIL;
      return err;
   }

   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A0, size), x, size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D0, size), m, size);

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);

   entry_pnt = ELP_CLUE_ENTRY_MODINV;

   // GO
   err = pka_core_go(state, entry_pnt, size);
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
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, C0, size), result, size);

DONE:
   if (pka_driv_unlock() != PKA_OK)
   {
      PKA_REPORT("unlock fail");
   }

   return err;
}
*/

int pka_core_ecc_modadd(struct pka_state *state, uint8_t *x,  uint32_t xlen, uint8_t *y, uint8_t *m, uint8_t *result, uint16_t size)
{
   uint32_t entry_pnt = 0;
   int err = PKA_ERR;
   uint8_t tmp[PKA_CURVE_NIST_P256_BYTE] = {0};

   if (pka_driv_lock() != PKA_OK)
   {
      PKA_REPORT("lock fail");
      err = PKA_LOCKFAIL;
      return err;
   }

   if (xlen < size)
   {
      if( size > sizeof(tmp)) {
         PKA_REPORT_ERR("pka_core_ecc_modadd - invalid size", size);
         return PKA_INVPARAM;
      }
      memcpy(tmp+(size-xlen), x, xlen);
      pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A0, size), tmp, size);
   }
   else
   {
      pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A0, size), x, size);
   }
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, B0, size), y, size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D0, size), m, size);

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);

   entry_pnt = ELP_CLUE_ENTRY_MODADD;

   // GO
   err = pka_core_go(state, entry_pnt, size);
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
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A0, size), result, size);

DONE:
   if (pka_driv_unlock() != PKA_OK)
   {
      PKA_REPORT("unlock fail");
   }
   return err;
}

int pka_core_ecc_moddiv(struct pka_state *state, uint8_t *x, uint32_t xlen, uint8_t *y, uint8_t *m, uint8_t *result, uint16_t size)
{
   uint32_t entry_pnt = 0;
   int err = PKA_ERR;
   uint8_t tmp[PKA_CURVE_NIST_P256_BYTE] = {0};

   if (xlen < size)
   {
      if( size > sizeof(tmp)) {
         PKA_REPORT_ERR("pka_core_ecc_modadd - invalid size", size);
         return PKA_INVPARAM;
      }
      memcpy(tmp+(size-xlen), x, xlen);
   }

   if (pka_driv_lock() != PKA_OK)
   {
      PKA_REPORT("lock fail");
      err = PKA_LOCKFAIL;
      return err;
   }

   if (xlen < size)
   {
      pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, C0, size), tmp, size);
   }
   else
   {
      pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, C0, size), x, size);
   }

   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A0, size), y, size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D0, size), m, size);

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);

   entry_pnt = ELP_CLUE_ENTRY_MODDIV;

   // GO
   err = pka_core_go(state, entry_pnt, size);
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
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, C0, size), result, size);

DONE:
   if (pka_driv_unlock() != PKA_OK)
   {
      PKA_REPORT("unlock fail");
   }
   return err;
}

int pka_core_ecc_modsub(struct pka_state *state, uint8_t *x, uint8_t *y, uint8_t *m, uint8_t *result, uint16_t size)
{
   uint32_t entry_pnt = 0;
   int err = PKA_ERR;

   if (pka_driv_lock() != PKA_OK)
   {
      PKA_REPORT("lock fail");
      err = PKA_LOCKFAIL;
      return err;
   }

   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A0, size), x, size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, B0, size), y, size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D0, size), m, size);

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);

   entry_pnt = ELP_CLUE_ENTRY_MODSUB;

   // GO
   err = pka_core_go(state, entry_pnt, size);
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
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, A0, size), result, size);

DONE:
   if (pka_driv_unlock() != PKA_OK)
   {
      PKA_REPORT("unlock fail");
   }

   return err;
}

int pka_core_ecc_reduce(struct pka_state *state, uint8_t *x, uint8_t *m, uint8_t *result, uint16_t size)
{
   int err = PKA_ERR;
   uint32_t entry_pnt = 0;

   if (pka_driv_lock() != PKA_OK)
   {
      PKA_REPORT("lock fail");
      err = PKA_LOCKFAIL;
      return err;
   }

   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, C0, size), x, size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D0, size), m, size);

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);

   entry_pnt = ELP_CLUE_ENTRY_REDUCE;

   // GO
   err = pka_core_go(state, entry_pnt, size);
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
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, C0, size), result, size);

DONE:
   if (pka_driv_unlock() != PKA_OK)
   {
      PKA_REPORT("unlock fail");
   }

   return err;
}

int pka_core_ecc_mod(struct pka_state *state, uint8_t *x, uint8_t *m, uint8_t *result, uint16_t size)
{
   int err = PKA_ERR;
   uint32_t entry_pnt = 0;

   if (pka_driv_lock() != PKA_OK)
   {
      PKA_REPORT("lock fail");
      err = PKA_LOCKFAIL;
      return err;
   }

   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, C0, size), x, size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D0, size), m, size);

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);

   entry_pnt = ELP_CLUE_ENTRY_BIT_SERIAL_MOD;

   // GO
   err = pka_core_go(state, entry_pnt, size);
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
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, C0, size), result, size);

DONE:
   if (pka_driv_unlock() != PKA_OK)
   {
      PKA_REPORT("unlock fail");
   }

   return err;
}

int pka_core_ecc_mod_dp(struct pka_state *state, uint8_t *x, uint8_t *m, uint8_t *result, uint16_t size)
{
   int err = PKA_ERR;
   uint32_t entry_pnt = 0;

   if (pka_driv_lock() != PKA_OK)
   {
      PKA_REPORT("lock fail");
      err = PKA_LOCKFAIL;
      return err;
   }

   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, C0, size), x + size, size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, C1, size), x, size);
   pka_driv_write_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, D0, size), m, size);

   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 0);

   entry_pnt = ELP_CLUE_ENTRY_BIT_SERIAL_MOD_DP;

   // GO
   err = pka_core_go(state, entry_pnt, size);
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
   pka_driv_read_bigint((uint32_t)state->base+pka_core_ecc_reg_map(state, C0, size), result, size);

DONE:
   if (pka_driv_unlock() != PKA_OK)
   {
      PKA_REPORT("unlock fail");
   }

   return err;
}
