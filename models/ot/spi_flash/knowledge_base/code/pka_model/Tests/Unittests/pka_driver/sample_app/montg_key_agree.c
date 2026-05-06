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
 * \defgroup montg_key_agree  montg_key_agree.c
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

// retries
#define MAX_RETRIES 10

// key size
#define SIZE pka_curve_size[PKA_SW_CURVE_CURVE25519]


/* Montgomery key agreement simulated between two parties a and b
   - generate public key a from private key a
   - generate public key b from private key b
   - Acting as a,create shared secret using key priv a and key pub b
   - Acting as b,  create shared secret using key priv b and key pub a
   - Verify key_secret_a against key_secret_b
   - This sample app uses the random data for private key,if the random data input is out of rang,
     it will retry MAX_RETRIES times to get a valid random value.
*/
int montg_key_agree_main(void)
{
   struct pka_state pka;
   uint8_t key_priv_a[SIZE];
   uint8_t key_pub_a[SIZE];
   uint8_t blind_a[SIZE];
   uint8_t key_secret_a[SIZE];
   uint8_t key_priv_b[SIZE];
   uint8_t key_pub_b[SIZE];
   uint8_t blind_b[SIZE];
   uint8_t key_secret_b[SIZE];
   uint8_t u[SIZE];

   int i   = 0;
   int err = PKA_ERR;
   int pass_fail = 1;

   memset(u, 0, SIZE);
   u[SIZE-1] = 9;

   printf("Start Montgomery Key Agreement Sample App\n");

   // Init pka instance
   memset(&pka, 0, sizeof(struct pka_state));

   // Initialize
   err = pka_lib_setup(&pka, PKA_BASE_ADDRESS, (uint8_t *)ram_fw_bin, ram_fw_bin_len);

   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }


   // generate public key from private key
   pka_random(blind_a, SIZE);
   for (i=0; i<MAX_RETRIES; i++)
   {
      pka_random(key_priv_a, SIZE);
      err = pka_x25519(&pka, PKA_SW_CURVE_CURVE25519, key_priv_a, u, blind_a, key_pub_a);
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
   pka_random(blind_b, SIZE);
   for (i=0; i<MAX_RETRIES; i++)
   {
      pka_random(key_priv_b, SIZE);
      err = pka_x25519(&pka, PKA_SW_CURVE_CURVE25519, key_priv_b, u, blind_b, key_pub_b);
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
   // create shared secret using key priv a and key pub b
   err = pka_x25519(&pka, PKA_SW_CURVE_CURVE25519, key_priv_a, key_pub_b, blind_a, key_secret_a);

   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }


   // Acting as b
   // create shared secret using key priv b and key pub a
   err = pka_x25519(&pka, PKA_SW_CURVE_CURVE25519, key_priv_b, key_pub_a, blind_b, key_secret_b);

   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }


   // Verify key_secret_a against key_secret_b
   for (i=0; i < SIZE; i++)
   {
      if (key_secret_a[i] != key_secret_b[i])
      {
         printf("a[%d]=0x%x  b[%d]=0x%x\n", i, key_secret_a[i], i, key_secret_b[i]);
         err = PKA_VERFYFAIL;
      }
   }

   if (err == PKA_OK)
   {
      printf("montg key agree passed\n");
      pass_fail = 0;
   }
   else
   {
      printf("montg key agree failed\n");
   }


DONE:
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
   return montg_key_agree_main();
}
#endif

/**
 * \endcode
 *
 * @} End SampleApps
 */
