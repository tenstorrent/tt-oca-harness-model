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
 * Copyright (c) 2019-2020, 2022 Synopsys, Inc. and/or its affiliates.
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

/**
 * \addtogroup SampleApps
 * @{
 *
 * \defgroup ecc_sign_verify_bp_p384r1  ecc_sign_verify_bp_p384r1.c
 *
 *
 * \code
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>

#include "ram_fw.h"
#include "pka.h"

#define PKA_BASE_ADDRESS 0

// key generate and sign hash retries
#define MAX_RETRIES 10

// key size
#define SIZE pka_curve_size[PKA_SW_CURVE_BRAINPOOL_P384R1]

// SHA384 of message
uint8_t SHA384[] = {
  0x68, 0xe7, 0xe5, 0x13, 0x2d, 0x2d, 0x59, 0x85, 0xfc, 0x0c, 0x12, 0xf7,
  0x87, 0xed, 0x39, 0x33, 0xfa, 0x96, 0xbf, 0xc4, 0xdd, 0x0e, 0x5f, 0xef,
  0xd3, 0x33, 0x36, 0x83, 0x6d, 0x2e, 0xff, 0x85, 0xa6, 0x52, 0x27, 0x5e,
  0x1c, 0xfd, 0x10, 0xf2, 0x76, 0xe1, 0xc2, 0xf5, 0x1c, 0x6d, 0x9b, 0x13
};
unsigned int SHA384_len = 48;

#define ALLOC_MEMBLOCK(mem_ptr, SIZE)       (mem_ptr =  malloc(SIZE))
#define MEM_BLOCK_SIZE(SIZE)                ((SIZE * 4))

/* sign verify BP_P384R1 Sample App
   - Create a key pair with pka_ecc_key_generate().
   - Sign the message hash with pka_ecc_sign_hash()
   - Verify the signed hash with pka_ecc_verify_hash()
   - This sample app uses the random data for private key,if the random data input is out of rang,
     it will retry MAX_RETRIES times to get a valid random value.
*/
int ecc_sign_verify_bp_p384r1_main(void)
{
   struct pka_state pka;
   // private key
   uint8_t key_priv[SIZE];
   // blinding blind
   uint8_t blind[SIZE];
   // random
   uint8_t rng[SIZE];
   // public key
   struct ecc_point key_pub;
   uint8_t *sig_r;
   uint8_t *sig_s;

   uint8_t  *mem_block;

   int err = PKA_ERR;
   int i    = 0;
   int pass_fail = 1;

   printf("Start ecc sign verify BP_P384R1 Sample App\n");

   // Init pka instance
   memset(&pka, 0, sizeof(struct pka_state));

   // Initialize
   err = pka_lib_setup(&pka, PKA_BASE_ADDRESS, (uint8_t *)ram_fw_bin, ram_fw_bin_len);
   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE1;
   }

   // malloc  a block of memory with the size of all buffer needed
   err = (int)ALLOC_MEMBLOCK( mem_block , MEM_BLOCK_SIZE(SIZE));
   if (err == 0)
   {
       printf("%s line %d malloc fail SIZE=0x%x\n", __FUNCTION__, __LINE__, MEM_BLOCK_SIZE(SIZE));
       goto DONE1;
   }
   memset(mem_block, 0, MEM_BLOCK_SIZE(SIZE));

   // point buffers to memory block
   key_pub.x = mem_block;
   key_pub.y = mem_block + SIZE;
   sig_r     = mem_block + 2 * SIZE;
   sig_s     = mem_block + 3 * SIZE;

   // generate public key using private key
   pka_random(blind, SIZE);
   for (i=0; i<MAX_RETRIES; i++)
   {
       pka_random(key_priv, SIZE);
      err = pka_ecc_key_generate(&pka, PKA_SW_CURVE_BRAINPOOL_P384R1, key_priv, blind, key_pub);
      if (err != PKA_RC_REASON_INVALID_KEY)
      {
         break;
      }
   }
   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }

   for (i=0; i<MAX_RETRIES; i++)
   {
      pka_random(rng, SIZE);
      err = pka_ecc_sign_hash(&pka, PKA_SW_CURVE_BRAINPOOL_P384R1, key_priv, blind, rng, SHA384, SHA384_len, sig_r, sig_s);
      if (err != PKA_RC_REASON_INVALID_KEY)
      {
         break;
      }
   }
   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }

   err = pka_ecc_verify_hash(&pka, PKA_SW_CURVE_BRAINPOOL_P384R1, key_pub, SHA384, SHA384_len, sig_r, sig_s);
   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      printf("Sign and verify failed \n");
   }
   else
   {
      printf("Sign and verify passed \n");
      pass_fail = 0;
   }

DONE:
   free(mem_block);

DONE1:
   err = pka_lib_close(&pka);
   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      return err;
   }

   return pass_fail;
}

#if 0
int main(int argc, char **argv)
{
   return ecc_sign_verify_bp_p384r1_main();
}
#endif

/**
 * \endcode
 *
 * @} End SampleApps
 */
