
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

/**
 * \addtogroup SampleApps
 * @{
 *
 * \defgroup sm2_encrypt_decrypt_256  sm2_encrypt_decrypt_256.c
 *
 *
 * \code
 */

#define PKA_BASE_ADDRESS 0

// pka_sm2_key_generate retries
#define MAX_RETRIES 10

// key size
#define SIZE pka_curve_size[PKA_SW_CURVE_SM2_SCA256]

// message
static uint8_t Msg[] = {
  0xa7, 0xc3, 0x09, 0xd4, 0x4a, 0x57, 0x18, 0x8b, 0xbd, 0x7b, 0x72, 0x6b,
  0x98, 0xb9, 0x8c, 0xe1, 0x25, 0x82, 0x22, 0x8e, 0x14, 0x15, 0x86, 0x48,
  0x70, 0xa2, 0x39, 0x61, 0xd2, 0xaf, 0xb8, 0x2c, 0xd5, 0xbc, 0x98, 0xbe,
  0xc9, 0x22, 0xd5, 0xf2, 0xac, 0x41, 0x68, 0xb0, 0x56, 0xda, 0x17, 0x6e,
  0xf3, 0xba, 0x91, 0xf6, 0xb6, 0x99, 0xba, 0x6a, 0xcc, 0x41, 0x44, 0x86,
  0x8f, 0xf3, 0x7f, 0x26, 0xfd, 0x06, 0x72, 0x08, 0x68, 0xd1, 0x2a, 0xd2,
  0x6e, 0xcb, 0x52, 0x57, 0x2c, 0xf1, 0x04, 0x16, 0xaf, 0x68, 0xdf, 0x03,
  0xab, 0x64, 0x5a, 0x8b, 0x70, 0x48, 0x57, 0xd2, 0x19, 0x0f, 0xfc, 0x3f,
  0x07, 0xea, 0xbe, 0x3a, 0x8e, 0x2a, 0xbe, 0x34, 0xed, 0x61, 0x59, 0xe8,
  0x84, 0xc4, 0xfa, 0xe1, 0x41, 0xd4, 0x33, 0x3d, 0x5c, 0x3e, 0x0d, 0xb0,
  0x44, 0xff, 0x9c, 0xcc, 0xd9, 0xcb, 0xd6, 0x7f
};
static unsigned int Msg_len = 128;

#define ALLOC_MEMBLOCK(mem_ptr, size)       (mem_ptr =  malloc(size))
#define MEM_BLOCK_SIZE(SIZE)                (6*SIZE + 1 + 2*Msg_len)

// This is taken from GM/T 0003-2012
// Public key cryptographic algorithm SM2 based on elliptic curves
// Part 4: Public key encryption algorithm


/* simulates encrypted message transmission from party a to party b.
   - Creates a key pair for party b, Kb(pub, priv) with pka_sm2_key_generate().
   - Acting as a, call pka_ecc_point_validate() on Kb_pub.
   - Acting as a, call pka_sm2_encrypt() with Kb_pub, and a message (M) to create an ECIES encrypted message (EEM) for b
   - Acting as b, call pka_sm2_decrypt() with Kb_priv and the EEM to validate and extract the message (M?).
   - Shows that M? is equal to M.
   - This sample app uses the random data for private key. If the random data input is out of range,
     it will retry MAX_RETRIES times to get a valid random value.
*/
int sm2_encrypt_decrypt_256_main(void)
{

   struct pka_state pka;
   struct ecc_point key_pub_b;
   uint8_t key_priv_a[SIZE];
   uint8_t key_priv_b[SIZE];
   uint8_t blind[SIZE];

   uint8_t  *mem_block;
   uint8_t  *encdatat;
   uint8_t  *outdata;

   uint32_t encdatalen = 0;
   int      i;
   int err = PKA_ERR;
   int pass_fail = 1;

   printf("Start encrypt decrypt SM2_256 App\n");

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
   err = (int)ALLOC_MEMBLOCK(mem_block, MEM_BLOCK_SIZE(SIZE));

   if (err == 0)
   {
      printf("%s line %d malloc fail size=0x%x\n", __FUNCTION__, __LINE__, MEM_BLOCK_SIZE(SIZE));
      goto DONE1;
   }

   memset(mem_block, 0, MEM_BLOCK_SIZE(SIZE));

   key_pub_b.x     = mem_block;
   key_pub_b.y     = mem_block + SIZE;
   encdatat        = mem_block + 2*SIZE;
   outdata         = mem_block + 6*SIZE + Msg_len + 1;

   // generate public key b from private key b
   pka_random(blind, SIZE);
   for (i=0; i<MAX_RETRIES; i++)
   {
      pka_random(key_priv_b, SIZE);
      err = pka_sm2_key_generate(&pka, PKA_SW_CURVE_SM2_SCA256, key_priv_b,
                                 blind, SM2_SIGNING_KEY, key_pub_b);
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

   // Acting as a
   // simulate a receiving public key b by validating
   err = pka_ecc_point_validate(&pka, PKA_SW_CURVE_SM2_SCA256, key_pub_b);
   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }

   pka_random(blind, SIZE);
   pka_random(key_priv_a, SIZE);
   err = pka_sm2_encrypt(&pka, PKA_SW_CURVE_SM2_SCA256, key_pub_b, key_priv_a, blind, Msg, Msg_len, encdatat);
   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }

   encdatalen = 1 + (3*SIZE) + Msg_len;
   err = pka_sm2_decrypt(&pka, PKA_SW_CURVE_SM2_SCA256, key_priv_b, blind, encdatat, encdatalen, outdata);
   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }

   if (memcmp(outdata, Msg, Msg_len) != 0)
   {
      err = PKA_VERFYFAIL;
   }

   if (err == PKA_OK)
   {
      printf("encrypt decrypt passed\n");
      pass_fail = 0;
   }
   else
   {
      printf("encrypt decrypt failed\n");
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
   return sm2_encrypt_decrypt_256_main();
}
#endif

/**
 * \endcode
 *
 * @} End SampleApps
 */
