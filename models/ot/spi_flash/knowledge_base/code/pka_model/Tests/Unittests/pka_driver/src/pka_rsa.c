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
#include <stdlib.h>
#include <string.h>
#include "pka.h"
#include "pka_core_rsa.h"
#include "pka_hw.h"

struct pka_hash_len_byte {
   enum  hashType htype;
   uint8_t hashlen;
};

struct elp_pkcs1_data_table {
   enum  hashType htype;
   uint32_t derlen;
   uint8_t derhdr[20];
};

const struct pka_hash_len_byte hash_len[4] = {
      { PKA_HASH_SHA224, 28},
      { PKA_HASH_SHA256, 32},
      { PKA_HASH_SHA384, 48},
      { PKA_HASH_SHA512, 64}
};

// the DER encoding T  values  as defined in  (RFC 8017 - 9.2)
const struct elp_pkcs1_data_table elp_pkcs1_headers[4] = {
  { PKA_HASH_SHA224, 19, { 0x30, 0x2d, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x04, 0x05, 0x00, 0x04, 0x1C } },
  { PKA_HASH_SHA256, 19, { 0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20 } },
  { PKA_HASH_SHA384, 19, { 0x30, 0x41, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02, 0x05, 0x00, 0x04, 0x30 } },
  { PKA_HASH_SHA512, 19, { 0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03, 0x05, 0x00, 0x04, 0x40 } },

};

int pka_rsa_precomp(struct pka_state *state, struct pka_rsa_mod *mod, uint16_t  modlen)
{
   int err = PKA_ERR;

   err = pka_core_rsa_precomp(state, mod, modlen);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_rsa_precomp", err);
   }
   return err;
}

int pka_rsa_sign(struct pka_state *state, void *key, enum pka_rsa_key_format format, uint8_t *hash, enum hashType htype, uint8_t *signature)
{
   uint8_t   tlen, err = 0;
   uint16_t  modulus_bytelen, pslen = 0;
   uint8_t   em_buf[PKA_RSA_KEY_SIZE_BYTES] = {0};
   uint8_t   *em = em_buf;

   if (htype > PKA_HASH_SHA512)
   {
      PKA_REPORT_ERR("Invalid htype", htype);
      return PKA_INVPARAM;
   }

   if (format > RSA_CRT_KEY || format < RSA_KEY)
   {
      PKA_REPORT_ERR("Invalid format", format);
      return PKA_INVPARAM;
   }

   //the DER encoding T length as defined in  (RFC 8017 - 9.2)
   tlen = elp_pkcs1_headers[htype].derlen + hash_len[htype].hashlen;

   if (format == RSA_KEY)
   {
      modulus_bytelen = ((struct pka_rsa_key *)key)->mod_size;
   }
   else //format == RSA_CRT_KEY)
   {
      modulus_bytelen = ((struct pka_rsa_crt_key *)key)->mod_size;
   }
   if (PKA_RSA_KEY_SIZE_BYTES < modulus_bytelen)
   {
      PKA_REPORT_ERR("Unsupported key size", modulus_bytelen);
      return PKA_INVPARAM;
   }

   if (8 + 3 + tlen > modulus_bytelen)
   {
      PKA_REPORT("Invalid PKCS #1 parameters");
      return PKA_INVPARAM;
   }

   //PS length as defined in  (RFC 8017 - 9.2)
   pslen = modulus_bytelen - 3 - tlen;

   //  EME-PKCS1-v1_5 encoding
   //  EM = 0x00 || 0x01 || PS || 0x00 || T.

   em[0] = 0x00;
   em[1] = 0x01;
   memset(em+2, 0xFF, pslen);
   em[2+ pslen] =0x00 ;
   memcpy((em + 2+ pslen +1), elp_pkcs1_headers[htype].derhdr , elp_pkcs1_headers[htype].derlen);
   memcpy((em + 2+ pslen +1 + elp_pkcs1_headers[htype].derlen), hash , hash_len[htype].hashlen);

   if (format == RSA_KEY)
   {
      err = pka_core_rsa_modexp(state, key, em, signature);
      if (err != PKA_OK)
      {
         PKA_REPORT_ERR("pka_core_rsa_modexp", err);
      }
   }
   else  //format == RSA_CRT_KEY)
   {
      err = pka_core_rsa_modexp_crt(state, key, em, signature);
      if (err != PKA_OK)
      {
         PKA_REPORT_ERR("pka_core_rsa_modexp_crt", err);
      }
   }

   return err;
}

int pka_rsa_verify(struct pka_state *state, struct pka_rsa_key *key, uint8_t *hash, enum hashType htype, uint8_t *signature)
{
   uint8_t   tlen, err = 0;
   uint32_t  pslen = 0;
   uint8_t   em_buf[PKA_RSA_KEY_SIZE_BYTES] = {0};
   uint8_t   *em = em_buf;
   uint8_t   tmpsign[PKA_RSA_KEY_SIZE_BYTES] = {0};

   if (PKA_RSA_KEY_SIZE_BYTES < key->mod_size)
   {
      PKA_REPORT_ERR("Unsupported key size", key->mod_size);
      return PKA_INVPARAM;
   }

   if (htype > PKA_HASH_SHA512)
   {
      PKA_REPORT_ERR("Invalid hash type", htype);
      return PKA_INVPARAM;
   }

   //the DER encoding T length as defined in  (RFC 8017 - 9.2)
   tlen = elp_pkcs1_headers[htype].derlen + hash_len[htype].hashlen;

   if (8 + 3 + tlen> key->mod_size)
   {
      PKA_REPORT("Invalid PKCS #1 parameters");
      return PKA_INVPARAM;
   }

   //PS length as defined in  (RFC 8017 - 9.2)
   pslen = key->mod_size - 3 - tlen;

   //  EME-PKCS1-v1_5 encoding (RFC 8017 - 9.2)
   //  EM = 0x00 || 0x01 || PS || 0x00 || T
   em[0] = 0x00;
   em[1] = 0x01;
   memset(em+2, 0xFF, pslen);
   em[2+ pslen] = 0x00 ;
   memcpy((em + 2 + pslen + 1),elp_pkcs1_headers[htype].derhdr ,elp_pkcs1_headers[htype].derlen);
   memcpy((em + 2 + pslen + 1 + elp_pkcs1_headers[htype].derlen),hash ,hash_len[htype].hashlen);

   err = pka_core_rsa_modexp(state, key, signature, tmpsign);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_rsa_modexp", err);
   } else if (memcmp(tmpsign, em, key->mod_size) != 0 )
   {
      PKA_REPORT("Signature Verification Failed");
      err = PKA_VERFYFAIL;
   }

   return err;
}

int pka_rsa_encrypt(struct pka_state *state, struct pka_rsa_key *key , uint8_t *m, uint32_t mlen, uint8_t *rng, uint16_t rnglen, uint8_t *ct)
{
   uint32_t ps_size;
   uint8_t *em;
   uint32_t k = key->mod_size;  // RSA public key length of modulus n
   int err = PKA_ERR;

   // RSAES-PKCS1-V1_5-ENCRYPT
   if (mlen > k - 11)
   {
      PKA_REPORT_ERR("message too long", mlen);
      return PKA_INVRNGE;

   }

   // EME-PKCS1-v1_5 encoding
   // PS (Padding String)
   ps_size = k - mlen - 3;
   if ((ps_size) < 8)
   {
      PKA_REPORT_ERR("ps size too small", ps_size);
      return PKA_INVPARAM;
   }
   if (ps_size > rnglen)
   {
      PKA_REPORT_ERR("rnglen too small", rnglen);
      return PKA_INVPARAM;
   }
   //add padding
   em = malloc(k);
   if (!em)
   {
      PKA_REPORT_ERR("malloc fail size", k);
      return err;
   }
   em[0] = 0x00;
   em[1] = 0x02;
   // fill in rng
   memcpy(em + 2, rng, ps_size);
   em[ps_size+2] = 0x00;
   // fill in message
   memcpy(em + ps_size + 3, m, mlen);

   // RSA encryption
   err = pka_core_rsa_modexp(state, key, em, ct);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_rsa_modexp", err);
   }

   free(em);

   return err;
}
int pka_rsa_decrypt(struct pka_state *state, void *key, enum pka_rsa_key_format format, uint8_t *ct, uint8_t *msg, uint32_t msglen)
{
   uint32_t k;
   uint32_t ps_size;
   uint8_t *em;
   int err = PKA_ERR;

   // RSAES-PKCS1-V1_5-DECRYPT

   if (format == RSA_KEY)
   {
      struct pka_rsa_key *rsa_key = (struct pka_rsa_key *)key;
      k = rsa_key->mod_size;

      if (k < 11)
      {
         PKA_REPORT_ERR("fail key size", k);
         return PKA_INVPARAM;
      }

      em = malloc(k);

      if (!em)
      {
         PKA_REPORT_ERR("malloc fail size", k);
         return PKA_ERR;
      }

      // RSA decryption
      err = pka_core_rsa_modexp(state, rsa_key, ct, em);

      if (err != PKA_OK)
      {
         PKA_REPORT_ERR("pka_core_rsa_modexp", err);
         free(em);
         return err;
      }
   }
   else if (format == RSA_CRT_KEY)
   {
      struct pka_rsa_crt_key *rsa_crt_key = (struct pka_rsa_crt_key *)key;
      k = rsa_crt_key->mod_size;

      if (k < 11)
      {
         PKA_REPORT_ERR("fail key size", k);
         return PKA_INVPARAM;
      }

      em = malloc(k);

      if (!em)
      {
         PKA_REPORT_ERR("malloc fail size", k);
         return PKA_ERR;
      }

      // RSA CRT decryption
      err = pka_core_rsa_modexp_crt(state, rsa_crt_key, ct, em);

      if (err != PKA_OK)
      {
         PKA_REPORT_ERR("pka_core_rsa_modexp_crt", err);
         free(em);
         return err;
      }
   }
   else
   {
      PKA_REPORT_ERR("%s line %d fail format = 0x%x\n", format);
      return PKA_INVPARAM;
   }

   // check the fixed padding but not the ps
   ps_size = k - msglen - 3;

   if (ps_size < 8)
   {
      PKA_REPORT_ERR("fail ps size", ps_size);
      free(em);
      return PKA_INVPARAM;
   }

   if (!((em[0] == 0) &&
         (em[1] == 0x02) &&
         (em[ps_size + 2] == 0x00)))
   {
      PKA_REPORT_ERR("padding fail em[0]", em[0]);
      PKA_REPORT_ERR("padding fail em[1]", em[1]);
      PKA_REPORT_ERR("padding fail em[ps_size + 2]", em[ps_size + 2]);
      err = PKA_ERR;
      // do not return
   }
   else
   {
      // copy decrypted message
      memcpy(msg, em + ps_size + 3, msglen);
   }

   free(em);
   return err;
}
