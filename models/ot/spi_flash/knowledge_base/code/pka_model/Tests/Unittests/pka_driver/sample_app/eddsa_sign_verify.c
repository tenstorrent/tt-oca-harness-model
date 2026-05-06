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
 * Copyright (c) 2020, 2023 Synopsys, Inc. and/or its affiliates.
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
 * \defgroup  eddsa_sign_verify  eddsa_sign_verify.c
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

// key SIZE
#define SIZE pka_curve_size[PKA_SW_CURVE_ED25519]

// Taken from RFC 8032 7.1 Test Vectors for Ed25519
// -----TEST 3
uint8_t keypriv[] = {
   0xc5, 0xaa, 0x8d, 0xf4, 0x3f, 0x9f, 0x83, 0x7b, 0xed, 0xb7, 0x44, 0x2f,
   0x31, 0xdc, 0xb7, 0xb1, 0x66, 0xd3, 0x85, 0x35, 0x07, 0x6f, 0x09, 0x4b,
   0x85, 0xce, 0x3a, 0x2e, 0x0b, 0x44, 0x58, 0xf7
};
uint32_t keypriv_len = 32;


// Message
static uint8_t msg[] = {
   0xaf, 0x82
};
static uint32_t msg_len = 2;

/* eddsa sign verify Sample App
   - Create a key pair with pka_eddsa_key_gen().
   - Sign the message hash with pka_eddsa_sign()
   - Verify the signed hash with pka_eddsa_verify()
*/
int eddsa_sign_verify_main(void)
{
   struct  pka_state pka;
   uint8_t keypub[SIZE];
   uint8_t sig_r[SIZE];
   uint8_t sig_s[SIZE];
   uint8_t blind[SIZE];
   int     pass_fail = 1;
   int     err = PKA_ERR;

   printf("Start Edwards sign verify Sample App\n");

   // Init pka instance
   memset(&pka, 0, sizeof(struct pka_state));

   // Initialize
   err = pka_lib_setup(&pka, PKA_BASE_ADDRESS, (uint8_t *)ram_fw_bin, ram_fw_bin_len);

   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }

   // generate public key
   pka_random(blind, SIZE);
   err = pka_eddsa_key_gen(&pka, PKA_SW_CURVE_ED25519, keypriv, blind, keypub);
   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }

   // sig_r, sig_s signature
   err = pka_eddsa_sign(&pka, PKA_SW_CURVE_ED25519,
                         keypriv, keypub, blind, msg, msg_len, sig_r, sig_s);

   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
      goto DONE;
   }


   // verify signature
   err = pka_eddsa_verify(&pka, PKA_SW_CURVE_ED25519, sig_r, sig_s, msg, msg_len, keypub);
   if (err != PKA_OK)
   {
      printf("%s line=%d err=%d\n", __FUNCTION__, __LINE__, err);
   }
   else
   {
      printf("Sign and verify passed \n");
      pass_fail = 0;
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
   return eddsa_sign_verify_main();
}
#endif

/**
 * \endcode
 *
 * @} End SampleApps
 */
