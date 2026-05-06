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
/**
 * \addtogroup SampleApps
 * @{
 *
 * \defgroup ecc_key_agree_nist384  ecc_key_agree_nist384.c
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
#include "sha512.h"

// hardware base address
#define PKA_BASE_ADDRESS 0

// pka_ecc_key_generate retries
#define KEY_GEN_MAX_RETRIES 10

// key size
#define SIZE pka_curve_size[PKA_SW_CURVE_NIST_P384]

#define ALLOC_MEMBLOCK(mem_ptr, SIZE)       (mem_ptr =  malloc(SIZE))
#define MEM_BLOCK_SIZE(SIZE)                ((8 * SIZE))


/* key agreement simulated between two parties a and b
   - generate public key a from private key a
   - generate public key b from private key b
   - Acting as a,simulate a receiving public key b by validating
       - create shared secret using key priv a and key pub b
       - create shared key a
   - Acting as b, simulate a receiving public key a by validating
       - create shared secret using key priv b and key pub a
       - create shared key b
   - Verify key_shared_a versus key_shared_b
   - This sample app uses the random data for private key,if the random data input is out of rang,
     it will retry MAX_RETRIES times to get a valid random value.
*/
int ecc_key_agree_nist384_main(void)
{
   // private key a
   uint8_t key_priv_a[SIZE];
   // private key b
   uint8_t key_priv_b[SIZE];
   // blinding blind
   uint8_t blind[SIZE];
   struct pka_state pka;
   struct ecc_point key_pub_a;
   struct ecc_point key_pub_b;
   struct ecc_point key_secret_a;
   struct ecc_point key_secret_b;

   uint8_t key_shared_a[SHA_512_SIZE];
   uint8_t key_shared_b[SHA_512_SIZE];

   uint8_t  *mem_block;

   int i    = 0;
   int err  = PKA_ERR;
   int pass_fail = 1;

   printf("Start Key Agreement NIST_384 Sample App\n");

   // Init pka instance
   memset(&pka, 0, sizeof(struct pka_state));

   // Initialize virtual addresses for hardware access
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
       printf("%s line %d malloc fail size=0x%x\n", __FUNCTION__, __LINE__, MEM_BLOCK_SIZE(SIZE));
       goto DONE1;
   }

   memset(mem_block, 0, MEM_BLOCK_SIZE(SIZE));

   // point buffers to memory block
   key_pub_a.x     = mem_block;
   key_pub_a.y     = mem_block + SIZE;
   key_pub_b.x     = mem_block + 2*SIZE;
   key_pub_b.y     = mem_block + 3*SIZE;
   key_secret_a.x  = mem_block + 4*SIZE;
   key_secret_a.y  = mem_block + 5*SIZE;
   key_secret_b.x  = mem_block + 6*SIZE;
   key_secret_b.y  = mem_block + 7*SIZE;

   // generate public key from private key
   pka_random(blind, SIZE);
   for (i=0; i<KEY_GEN_MAX_RETRIES; i++)
   {
      pka_random(key_priv_a, SIZE);
      err = pka_ecc_key_generate(&pka, PKA_SW_CURVE_NIST_P384, key_priv_a, blind, key_pub_a);
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

   // generate public key from private key
   pka_random(blind, SIZE);
   for (i=0; i<KEY_GEN_MAX_RETRIES; i++)
   {
      pka_random(key_priv_b, SIZE);
      err = pka_ecc_key_generate(&pka, PKA_SW_CURVE_NIST_P384, key_priv_b, blind, key_pub_b);
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

   // Acting as a
   // simulate a receiving public key b by validating
   err = pka_ecc_point_validate(&pka, PKA_SW_CURVE_NIST_P384, key_pub_b);

   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }


   // create shared secret using key priv a and key pub b
   pka_random(blind, SIZE);
   err = pka_ecc_key_agreement(&pka, PKA_SW_CURVE_NIST_P384, key_pub_b, key_priv_a, blind, key_secret_a);

   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }

   // create shared key a
   err = pka_sha512(key_secret_a.x, SIZE, key_shared_a);

   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }


   // Acting as b
   // simulate a receiving public key a by validating
   err = pka_ecc_point_validate(&pka, PKA_SW_CURVE_NIST_P384, key_pub_a);

   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }

   // create shared secret using key priv b and key pub a
   pka_random(blind, SIZE);
   err = pka_ecc_key_agreement(&pka, PKA_SW_CURVE_NIST_P384, key_pub_a, key_priv_b, blind, key_secret_b);

   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }

   // create shared key b
   err = pka_sha512(key_secret_b.x, SIZE, key_shared_b);

   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }


   // Verify key_shared_a versus key_shared_b
   for (i=0; i < SHA_512_SIZE; i++)
   {
      if (key_shared_a[i] != key_shared_b[i])
      {
         printf("key_shared_a[%d]=0x%x    key_shared_b[%d]=0x%x\n",
                i, key_shared_a[i], i, key_shared_b[i]);
         err = PKA_VERFYFAIL;
      }
   }

   if (err == PKA_OK)
   {
      printf("ecc key agree passed\n");
      pass_fail = 0;
   }
   else
   {
      printf("ecc key agree failed\n");
   }

DONE:
   free(mem_block);

DONE1:

   err = pka_lib_close(&pka);

   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
   }




   return pass_fail;
}

#if 0
int main(int argc, char **argv)
{
   return ecc_key_agree_nist384_main();
}
#endif

/**
 * \endcode
 *
 * @} End SampleApps
 */
