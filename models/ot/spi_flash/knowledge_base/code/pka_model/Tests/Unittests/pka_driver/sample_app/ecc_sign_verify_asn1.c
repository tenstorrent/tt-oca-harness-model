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
 * \defgroup ecc_sign_verify_asn1  ecc_sign_verify_asn1.c
 *
 *
 * \code
 */
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include "asn1_der.h"
#include "pka.h"
#include "ram_fw.h"
#include "pka.h"

#define PKA_BASE_ADDRESS 0

#define SIZE pka_curve_size[PKA_SW_CURVE_NIST_P256]
#define MAX_RETRIES 50

// Message
static uint8_t msg[] = {
   0xaf, 0x82
};
static uint32_t msg_len = 2;

/* sign verify asn1 Sample App
   - Create a key pair with pka_ecc_key_generate().
   - Sign the message hash with pka_ecc_sign_hash_der()
   - Verify the signed hash with pka_ecc_verify_hash_der()
   - This sample app uses the random data for private key,if the random data input is out of rang,
     it will retry MAX_RETRIES times to get a valid random value.
*/
int ecc_sign_verify_asn1_main(void)
{
   struct  pka_state pka;
   struct ecc_point keypub;
   uint8_t blind[SIZE];
   uint8_t keypriv[SIZE];
   int     err = PKA_ERR;
   uint8_t *signature;
   uint32_t sig_len;
   uint8_t i;

   keypub.x = malloc(SIZE);
   keypub.y = malloc(SIZE);
   sig_len = 512;
   signature = malloc(sig_len);
   i = 0;

   printf("Start ecc sign verify asn1  Sample App\n");
   // Initialize
   err = pka_lib_setup(&pka, PKA_BASE_ADDRESS, (uint8_t *)ram_fw_bin, ram_fw_bin_len);
   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }

   // generate public key
   pka_random(blind, SIZE);
   for(i=0;i<MAX_RETRIES;i++)
   {
      pka_random(keypriv, SIZE);
      err = pka_ecc_key_generate(&pka, PKA_SW_CURVE_NIST_P256, keypriv, blind, keypub);
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

   err = pka_ecc_sign_hash_der(&pka, PKA_SW_CURVE_NIST_P256,
                         keypriv, blind, blind, msg, msg_len, signature, &sig_len);

   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }

   // verify signature
   err = pka_ecc_verify_hash_der(&pka, PKA_SW_CURVE_NIST_P256, keypub, msg, msg_len, signature, sig_len);
   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
   }
   else
   {
      printf("Sign and verify passed \n");
   }

DONE:
   free(keypub.x);
   free(keypub.y);
   free(signature);
   err = pka_lib_close(&pka);

   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
   }


   return err;
}

#if 0
int main(int argc, char **argv)
{
   return ecc_sign_verify_asn1_main();
}
#endif

/**
 * \endcode
 *
 * @} End SampleApps
 */
