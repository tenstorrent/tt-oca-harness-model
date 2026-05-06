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
 * \defgroup  sm2_sign_verify_256  sm2_sign_verify_256.c
 *
 *
 * \code
 */

#define PKA_BASE_ADDRESS 0

// pka_sm2_key_generate and  pka_sm2_sign_hash retries
#define MAX_RETRIES 10

// key size
#define SIZE pka_curve_size[PKA_SW_CURVE_SM2_SCA256]

// This is taken from GM/T 0003-2012
// Public key cryptographic algorithm SM2 based on elliptic curves
// Part 5: Parameter Definition
// Annex A Example of digital signature and verification

// IDa identifier of user
/*
const uint8_t IDa[] = {
  0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
  0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38
};
unsigned int IDA_len = 16;
*/

//
// SM3 256 bit hash
// Za = SM3_HASH(ENTLa || IDa || a || b || xg || yg || xa || Ya )
// ENTLa = 0080
//
// Note: This is a pre-computed value used specifically for this example based on the
// parameters defined my the SM2 algorithm to compute Za. You must supply your own hash
// implementation and compute Za for your own parameters. The SM3 hash implementation
// is not provided as part of this reference code.
//
uint8_t Za[] = {
   0xb2, 0xe1, 0x4c, 0x5c, 0x79, 0xc6, 0xdf, 0x5b, 0x85, 0xf4, 0xfe, 0x7e,
   0xd8, 0xdb, 0x7a, 0x26, 0x2b, 0x9d, 0xa7, 0xe0, 0x7c, 0xcb, 0x0e, 0xa9,
   0xf4, 0x74, 0x7b, 0x8c, 0xcd, 0xa8, 0xa4, 0xf3
};
unsigned int Za_len = 32;

//
// Pre-computed message digest
//
/*
uint8_t M[] = {
   0x6d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x20, 0x64, 0x69, 0x67, 0x65,
   0x73, 0x74
};
unsigned int M_len = 14;
*/

//
// e = SM3_HASH(ZA || M)
//
// Note: This is also pre-computed hash value used specifically for this example based on the
// pre-computed Za and M(essage). You must supply your own hash implementation and compute
// the e value. The SM3 hash implementation is not provided as part of this reference code.
//
uint8_t e[] = {
   0xf0, 0xb4, 0x3e, 0x94, 0xba, 0x45, 0xac, 0xca, 0xac, 0xe6, 0x92, 0xed,
   0x53, 0x43, 0x82, 0xeb, 0x17, 0xe6, 0xab, 0x5a, 0x19, 0xce, 0x7b, 0x31,
   0xf4, 0x48, 0x6f, 0xdf, 0xc0, 0xd2, 0x86, 0x40
};
unsigned int e_len = 32;

// random number k
uint8_t k[] = {
   0x59, 0x27, 0x6e, 0x27, 0xd5, 0x06, 0x86, 0x1a, 0x16, 0x68, 0x0f, 0x3a,
   0xd9, 0xc0, 0x2d, 0xcc, 0xef, 0x3c, 0xc1, 0xfa, 0x3c, 0xdb, 0xe4, 0xce,
   0x6d, 0x54, 0xb8, 0x0d, 0xea, 0xc1, 0xbc, 0x21
};

// private key
// dA
uint8_t key_priv[] = {
   0x39, 0x45, 0x20, 0x8f, 0x7b, 0x21, 0x44, 0xb1, 0x3f, 0x36, 0xe3, 0x8a,
   0xc6, 0xd3, 0x9f, 0x95, 0x88, 0x93, 0x93, 0x69, 0x28, 0x60, 0xb5, 0x1a,
   0x42, 0xfb, 0x81, 0xef, 0x4d, 0xf7, 0xc5, 0xb8
};

#define ALLOC_MEMBLOCK(mem_ptr, SIZE)       (mem_ptr =  malloc(SIZE))
#define MEM_BLOCK_SIZE(SIZE)                ((SIZE * 4))

/* sign verify SM2 Curve 256 bits Sample App
   - Create a key pair with pka_sm2_key_generate().
   - Sign the message hash with pka_sm2_sign_hash().
   - Verify the signed hash with pka_sm2_verify_hash().
*/
int sm2_sign_verify_256_main(void)
{
   struct pka_state pka;
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

   printf("Start sign verify SM2_256 Sample App\n");

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
      //pka_random(key_priv, SIZE);
      err = pka_sm2_key_generate(&pka, PKA_SW_CURVE_SM2_SCA256, key_priv,
                                 blind, SM2_SIGNING_KEY, key_pub);
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

   for (i=0; i<MAX_RETRIES; i++)
   {
      pka_random(rng, SIZE);
      err = pka_sm2_sign_hash(&pka, PKA_SW_CURVE_SM2_SCA256, key_priv, rng, k, e, e_len, sig_r, sig_s);
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

   err = pka_sm2_verify_hash(&pka, PKA_SW_CURVE_SM2_SCA256, key_pub, e, e_len, sig_r, sig_s);
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
   }

   return pass_fail;
}

#if 0
int main(int argc, char **argv)
{
   return sm2_sign_verify_256_main();
}
#endif

/**
 * \endcode
 *
 * @} End SampleApps
 */
