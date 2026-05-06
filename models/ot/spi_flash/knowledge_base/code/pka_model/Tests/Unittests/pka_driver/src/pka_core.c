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
 * Copyright (c) 2020,2022 Synopsys, Inc. and/or its affiliates.
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
#include "pka_core.h"
#include "pka_core_rsa.h"
#include "pka_core_ecc.h"
#include "data_macros.h"


int pka_core_fw_write(struct pka_state *state, uint8_t *buffer, uint16_t length)
{
   uint16_t i;

   for (i = 0; i < length; i=i+4)
   {
      pka_driv_write((uint32_t)state->base + PKA_FW_AREA + i, SWAP32(*(uint32_t *)(void*)(buffer + i)));
   }

   PKA_REPORT("pka FW loaded");

   return PKA_OK;
}

int pka_core_init(struct pka_state *state, uint32_t pka_addr, uint8_t *buffer, uint16_t length)
{
   uint32_t pka_address;
   int err = PKA_ERR;

   // physical addresses input
   pka_address = pka_addr;

   // mmap physical to virtual addresses
   err = pka_driv_init(&pka_address);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("mmap fail", err);
      return PKA_INVPARAM;
   }

   // save the virtual addresses
   state->base = (uint32_t *)pka_address;

   // software version
   PKA_REPORT(software_version);

   // set hardware to big endian
   pka_driv_write((uint32_t)state->base + PKA_REG_CONFIG, 1 << PKA_CONF_BYTESWAP);
   // set blinding
   pka_driv_write((uint32_t)state->base + PKA_REG_FLAGS, 1 << PKA_FLAG_F0);
   state->build_cfg = pka_driv_read((uint32_t)state->base + PKA_REG_BUILD_CONFIG);
   // Hardware Configuration registers
   PKA_REPORT_ERR_HEX("BUILD_CONFIG", state->build_cfg);
   PKA_REPORT_ERR_HEX("pka CONFIG", pka_driv_read((uint32_t)state->base + PKA_REG_CONFIG));
   // load pka firmware
   if (buffer != NULL)
   {
      pka_core_fw_write(state, buffer, length);
      if (err != PKA_OK)
      {
         PKA_REPORT_ERR("fw write fail", err);
         return PKA_INVPARAM;
      }
   }

   return err;
}

int pka_core_wait(struct pka_state *state)
{
   // make sure hardware is not BUSY
   int max_retries = 0;
   while (((pka_driv_read((uint32_t)state->base + PKA_REG_RTN_CODE)) >> PKA_RC_BUSY) == 1 && max_retries < PKA_MAX_TRIES_ON_WAIT) {
       max_retries++;
   }
   PKA_REPORT_ERR_HEX("pka_core_wait tries: %d", max_retries);
   PKA_REPORT_ERR_HEX("instr_since_go: %d", pka_driv_read((uint32_t)state->base + PKA_REG_INSTR_SINCE_GO));
   if (max_retries >= PKA_MAX_TRIES_ON_WAIT)
   {
       PKA_REPORT("pka_core_wait timeout");
       return PKA_TIMEOUT;
   }
   // check for DONE
   pka_driv_wait((uint32_t)state->base + PKA_REG_STAT, (1 << PKA_STAT_DONE));
   // set RETURN_CODE
   return (((pka_driv_read((uint32_t)state->base + PKA_REG_RTN_CODE)) & PKA_RC_REASON_MASK) >> PKA_RC_REASON);
}

int pka_core_go(struct pka_state *state, uint32_t entry, uint16_t size)
{
   uint8_t  base_radix = 0;
   uint8_t  partial_radix = 0;
   uint32_t ctrl = 0;

   if (entry == (PKA_ENTRY_POINT_UNSUPPORTED & 0xFF))
   {
      PKA_REPORT_ERR("Entry Point Unsupported", PKA_FW_ENTRY_POINT_ERR);
      return PKA_FW_ENTRY_POINT_ERR;
   }

   PKA_REPORT_ERR_HEX("pka_core_go entry", entry);
   switch(size*8)
   {
   case 256:
      base_radix = base_radix_256;
      break;
   case 384:
      base_radix = base_radix_512;
      partial_radix = 12;
      break;
   case 512:
      base_radix = base_radix_512;
      break;
   case 528:
      base_radix = base_radix_1024;
      // partial radix using ALU width
      switch((state->build_cfg & PKA_BC_ALU_SZ_MASK) >> PKA_BC_ALU_SZ)
      {
         case ALU_32_BITS:
            partial_radix = 17;
            break;
         case ALU_64_BITS:
            partial_radix = 18;
            break;
         case ALU_128_BITS:
            partial_radix = 20;
            break;
         default:
            PKA_REPORT_ERR_HEX("Fail ALU width", (state->build_cfg & PKA_BC_ALU_SZ_MASK) >> PKA_BC_ALU_SZ);
            return PKA_INVRNGE;
            break;
      }
      break;
   case 768:
      base_radix = base_radix_1024;
      partial_radix = 24;
      break;
   case 1024:
      base_radix = base_radix_1024;
      break;
   case 1536:
      base_radix = base_radix_2048;
      partial_radix = 48;
      break;
   case 2048:
      base_radix = base_radix_2048;
      break;
   case 3072:
      base_radix = base_radix_4096;
      partial_radix = 96;
      break;
   case 4096:
      base_radix = base_radix_4096;
      break;
   default:
      PKA_REPORT_ERR("Fail size", size);
      return PKA_INVRNGE;
   }

   // clear stack ptr
   pka_driv_write((uint32_t)state->base + PKA_REG_STACK_PNTR, 0);
   // clear indexes
   pka_driv_write((uint32_t)state->base + PKA_REG_INDEX_I, 0);
   pka_driv_write((uint32_t)state->base + PKA_REG_INDEX_J, 0);
   pka_driv_write((uint32_t)state->base + PKA_REG_INDEX_K, 0);
   pka_driv_write((uint32_t)state->base + PKA_REG_INDEX_L, 0);

   // Write hardware entry point
   pka_driv_write((uint32_t)state->base + PKA_REG_ENTRY_PNT, entry);
   // clear PKA_REG_STAT
   pka_driv_write((uint32_t)state->base + PKA_REG_STAT, (1 << PKA_STAT_DONE));
   // GO using supplied size

   if(size == PKA_CURVE_NIST_P521_BYTE )
   {
       ctrl = base_radix << PKA_CTRL_BASE_RADIX;
       ctrl  |= PKA_CTRL_M521_ECC521 << PKA_CTRL_M521_MODE;
       ctrl |= partial_radix << PKA_CTRL_PARTIAL_RADIX;
       ctrl |= 1ul << PKA_CTRL_GO;
       pka_driv_write((uint32_t)state->base + PKA_REG_CTRL,ctrl);
   }
   else
   {
       pka_driv_write((uint32_t)state->base + PKA_REG_CTRL,
           (1 << PKA_CTRL_GO)
           | (base_radix << PKA_CTRL_BASE_RADIX)
           | (partial_radix << PKA_CTRL_PARTIAL_RADIX));
   }

   return PKA_OK;
}

