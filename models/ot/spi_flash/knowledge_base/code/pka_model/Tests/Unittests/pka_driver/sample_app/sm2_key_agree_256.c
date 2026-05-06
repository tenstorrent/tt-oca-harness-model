
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


#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include "ram_fw.h"
#include "pka.h"
#include "sm3.h"

/**
 * \addtogroup SampleApps
 * @{
 *
 * \defgroup sm2_key_agree_256  sm2_key_agree_256.c
 *
 *
 * \code
 */


// hardware base address
#define PKA_BASE_ADDRESS 0

// pka_ecc_key_generate retries
#define KEY_GEN_MAX_RETRIES 10

// key size
#define SIZE pka_curve_size[PKA_SW_CURVE_SM2_SCA256]

#define ALLOC_MEMBLOCK(mem_ptr, SIZE)       (mem_ptr =  malloc(SIZE))
#define MEM_BLOCK_SIZE(SIZE)                ((8 * SIZE))

char uid_a[] = {
   0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
   0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38
};
int  uid_a_len = 16;

char uid_b[] = {
   0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x39,
   0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x39
};
int  uid_b_len = 16;


// key agreement simulated between two parties a and b
int sm2_key_agree_256_main(void)
{
   // private key a
   uint8_t key_priv_a[SIZE];
   // ephemeral key a
   uint8_t key_eph_a[SIZE];
   // user_hash_a
   uint8_t user_hash_a[SM3_SIZE];
   uint32_t user_hash_a_len = SM3_SIZE;
   // private key b
   uint8_t key_priv_b[SIZE];
   // ephemeral key b
   uint8_t key_eph_b[SIZE];
   // user_hash_b
   uint8_t user_hash_b[SM3_SIZE];
   uint32_t user_hash_b_len = SM3_SIZE;
   // blinding blind
   uint8_t blind[SIZE];
   struct pka_state pka;
   struct ecc_point key_pub_a;
   struct ecc_point key_eph_pub_a;
   struct ecc_point key_pub_b;
   struct ecc_point key_eph_pub_b;

   uint8_t key_shared_a[SIZE];
   uint8_t key_shared_b[SIZE];

   uint8_t  *mem_block;

   int i    = 0;
   int err  = PKA_ERR;
   int pass_fail = 1;

   printf("Start Key Agreement SM2_256 Sample App\n");

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
   key_eph_pub_a.x = mem_block + 2*SIZE;
   key_eph_pub_a.y = mem_block + 3*SIZE;
   key_pub_b.x     = mem_block + 4*SIZE;
   key_pub_b.y     = mem_block + 5*SIZE;
   key_eph_pub_b.x = mem_block + 6*SIZE;
   key_eph_pub_b.y = mem_block + 7*SIZE;

   // generate public key from private key
   pka_random(blind, SIZE);
   for (i=0; i<KEY_GEN_MAX_RETRIES; i++)
   {
       pka_random(key_priv_a, SIZE);
      err = pka_sm2_key_generate(&pka, PKA_SW_CURVE_SM2_SCA256, key_priv_a,
                                 blind, SM2_KEY_AGREEMENT_KEY, key_pub_a);
      if ((err != PKA_RC_REASON_INVALID_KEY) && (err != PKA_INVRNGE))
      {
         break;
      }
   }
   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }

   // generate public ephemeral key from ephemeral key
   pka_random(blind, SIZE);
   for (i=0; i<KEY_GEN_MAX_RETRIES; i++)
   {
      pka_random(key_eph_a, SIZE);
      err = pka_sm2_key_generate(&pka, PKA_SW_CURVE_SM2_SCA256, key_eph_a,
                                 blind, SM2_KEY_AGREEMENT_KEY, key_eph_pub_a);
      if ((err != PKA_RC_REASON_INVALID_KEY) && (err != PKA_INVRNGE))
      {
         break;
      }
   }
   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }

   // make user hash
   err = pka_sm2_make_user_hash(uid_a, uid_a_len, key_pub_a, user_hash_a, &user_hash_a_len);
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
      err = pka_sm2_key_generate(&pka, PKA_SW_CURVE_SM2_SCA256, key_priv_b,
                                 blind, SM2_KEY_AGREEMENT_KEY, key_pub_b);
      if ((err != PKA_RC_REASON_INVALID_KEY) && (err != PKA_INVRNGE))
      {
         break;
      }
   }
   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }

   // generate public ephemeral key from ephemeral key
   pka_random(blind, SIZE);
   for (i=0; i<KEY_GEN_MAX_RETRIES; i++)
   {
      pka_random(key_eph_b, SIZE);
      err = pka_sm2_key_generate(&pka, PKA_SW_CURVE_SM2_SCA256, key_eph_b,
                                 blind, SM2_KEY_AGREEMENT_KEY, key_eph_pub_b);
      if ((err != PKA_RC_REASON_INVALID_KEY) && (err != PKA_INVRNGE))
      {
         break;
      }
   }
   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }

   // make user hash
   err = pka_sm2_make_user_hash(uid_b, uid_b_len, key_pub_b, user_hash_b, &user_hash_b_len);
   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }

   // Acting as a
   // simulate a receiving public key b by validating
   err = pka_ecc_point_validate(&pka, PKA_SW_CURVE_SM2_SCA256, key_pub_b);

   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }

   // create shared secret using key priv a and key pub b

   err = pka_sm2_key_agreement(&pka, PKA_SW_CURVE_SM2_SCA256,
                               key_pub_b, key_eph_pub_b, user_hash_b,
                               key_priv_a, key_eph_a, key_eph_pub_a, user_hash_a,
                               blind, SM2_INITIATOR, key_shared_a, SIZE);

   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }

   // Acting as b
   // simulate a receiving public key a by validating
   err = pka_ecc_point_validate(&pka, PKA_SW_CURVE_SM2_SCA256, key_pub_a);

   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }

   // create shared secret using key priv b and key pub a
   pka_random(blind, SIZE);
   err = pka_sm2_key_agreement(&pka, PKA_SW_CURVE_SM2_SCA256,
                               key_pub_a, key_eph_pub_a, user_hash_a,
                               key_priv_b, key_eph_b, key_eph_pub_b, user_hash_b,
                               blind, SM2_RESPONDER, key_shared_b, SIZE);

   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }


   // Verify key_shared_a versus key_shared_b
   for (i=0; i < SIZE/2; i++)
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
      printf("sm2 key agree passed\n");
      pass_fail = 0;
   }
   else
   {
      printf("sm2 key agree failed\n");
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
   return sm2_key_agree_256_main();
}
#endif

/**
 * \endcode
 *
 * @} End SampleApps
 */
