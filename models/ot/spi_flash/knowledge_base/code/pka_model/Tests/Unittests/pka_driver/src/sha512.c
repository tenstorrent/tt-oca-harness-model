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
 * Copyright (c) 2011-2020, 2022 Synopsys, Inc. and/or its affiliates.
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

 /*   SHA-512 implementation as defined in FIPS-180-2 */

#include "sha512.h"
#include "data_macros.h"

/* the K array */
static const uint64_t K[80] = {
CONST64(0x428a2f98d728ae22), CONST64(0x7137449123ef65cd),
CONST64(0xb5c0fbcfec4d3b2f), CONST64(0xe9b5dba58189dbbc),
CONST64(0x3956c25bf348b538), CONST64(0x59f111f1b605d019),
CONST64(0x923f82a4af194f9b), CONST64(0xab1c5ed5da6d8118),
CONST64(0xd807aa98a3030242), CONST64(0x12835b0145706fbe),
CONST64(0x243185be4ee4b28c), CONST64(0x550c7dc3d5ffb4e2),
CONST64(0x72be5d74f27b896f), CONST64(0x80deb1fe3b1696b1),
CONST64(0x9bdc06a725c71235), CONST64(0xc19bf174cf692694),
CONST64(0xe49b69c19ef14ad2), CONST64(0xefbe4786384f25e3),
CONST64(0x0fc19dc68b8cd5b5), CONST64(0x240ca1cc77ac9c65),
CONST64(0x2de92c6f592b0275), CONST64(0x4a7484aa6ea6e483),
CONST64(0x5cb0a9dcbd41fbd4), CONST64(0x76f988da831153b5),
CONST64(0x983e5152ee66dfab), CONST64(0xa831c66d2db43210),
CONST64(0xb00327c898fb213f), CONST64(0xbf597fc7beef0ee4),
CONST64(0xc6e00bf33da88fc2), CONST64(0xd5a79147930aa725),
CONST64(0x06ca6351e003826f), CONST64(0x142929670a0e6e70),
CONST64(0x27b70a8546d22ffc), CONST64(0x2e1b21385c26c926),
CONST64(0x4d2c6dfc5ac42aed), CONST64(0x53380d139d95b3df),
CONST64(0x650a73548baf63de), CONST64(0x766a0abb3c77b2a8),
CONST64(0x81c2c92e47edaee6), CONST64(0x92722c851482353b),
CONST64(0xa2bfe8a14cf10364), CONST64(0xa81a664bbc423001),
CONST64(0xc24b8b70d0f89791), CONST64(0xc76c51a30654be30),
CONST64(0xd192e819d6ef5218), CONST64(0xd69906245565a910),
CONST64(0xf40e35855771202a), CONST64(0x106aa07032bbd1b8),
CONST64(0x19a4c116b8d2d0c8), CONST64(0x1e376c085141ab53),
CONST64(0x2748774cdf8eeb99), CONST64(0x34b0bcb5e19b48a8),
CONST64(0x391c0cb3c5c95a63), CONST64(0x4ed8aa4ae3418acb),
CONST64(0x5b9cca4f7763e373), CONST64(0x682e6ff3d6b2b8a3),
CONST64(0x748f82ee5defb2fc), CONST64(0x78a5636f43172f60),
CONST64(0x84c87814a1f0ab72), CONST64(0x8cc702081a6439ec),
CONST64(0x90befffa23631e28), CONST64(0xa4506cebde82bde9),
CONST64(0xbef9a3f7b2c67915), CONST64(0xc67178f2e372532b),
CONST64(0xca273eceea26619c), CONST64(0xd186b8c721c0c207),
CONST64(0xeada7dd6cde0eb1e), CONST64(0xf57d4f7fee6ed178),
CONST64(0x06f067aa72176fba), CONST64(0x0a637dc5a2c898a6),
CONST64(0x113f9804bef90dae), CONST64(0x1b710b35131c471b),
CONST64(0x28db77f523047d84), CONST64(0x32caab7b40c72493),
CONST64(0x3c9ebe0a15c9bebc), CONST64(0x431d67c49c100d4c),
CONST64(0x4cc5d4becb3e42b6), CONST64(0x597f299cfc657e2a),
CONST64(0x5fcb6fab3ad6faec), CONST64(0x6c44198c4a475817)
};

/* Various logical functions */
#define Ch(x,y,z)       (z ^ (x & (y ^ z)))
#define Maj(x,y,z)      (((x | y) & z) | (x & y))
#define S(x, n)         ROR64c(x, n)
#define R(x, n)         (((x)&CONST64(0xFFFFFFFFFFFFFFFF))>>((uint64_t)n))
#define Sigma0(x)       (S(x, 28) ^ S(x, 34) ^ S(x, 39))
#define Sigma1(x)       (S(x, 14) ^ S(x, 18) ^ S(x, 41))
#define Gamma0(x)       (S(x, 1) ^ S(x, 8) ^ R(x, 7))
#define Gamma1(x)       (S(x, 19) ^ S(x, 61) ^ R(x, 6))

int pka_sha512_init(sha512_state *md)
{


   if (md == NULL)
   {
       PKA_REPORT("Invalid pointer\n");
       return PKA_SHA_ERR;
   }

    md->curlen = 0;
    md->length = 0;
    md->state[0] = CONST64(0x6a09e667f3bcc908);
    md->state[1] = CONST64(0xbb67ae8584caa73b);
    md->state[2] = CONST64(0x3c6ef372fe94f82b);
    md->state[3] = CONST64(0xa54ff53a5f1d36f1);
    md->state[4] = CONST64(0x510e527fade682d1);
    md->state[5] = CONST64(0x9b05688c2b3e6c1f);
    md->state[6] = CONST64(0x1f83d9abfb41bd6b);
    md->state[7] = CONST64(0x5be0cd19137e2179);
    return PKA_OK;
}
/* compress 1024-bits */
int pka_sha512_compress(sha512_state * md, uint8_t *buf)
{
   uint64_t S[8], W[80], t0, t1;
   int i;


   /* copy state into S */
   for (i = 0; i < 8; i++) {
       S[i] = md->state[i];
   }

   /* copy the state into 1024-bits into W[0..15] */
   for (i = 0; i < 16; i++) {
        LOAD64H(W[i], buf + (8*i));
   }

   /* fill W[16..79] */
   for (i = 16; i < 80; i++) {
       W[i] = Gamma1(W[i - 2]) + W[i - 7] + Gamma0(W[i - 15]) + W[i - 16];
   }

   /* Compress */

   for (i = 0; i < 80; i++) {
       t0 = S[7] + Sigma1(S[4]) + Ch(S[4], S[5], S[6]) + K[i] + W[i];
       t1 = Sigma0(S[0]) + Maj(S[0], S[1], S[2]);
       S[7] = S[6];
       S[6] = S[5];
       S[5] = S[4];
       S[4] = S[3] + t0;
       S[3] = S[2];
       S[2] = S[1];
       S[1] = S[0];
       S[0] = t0 + t1;
   }


   /* feedback */
   for (i = 0; i < 8; i++) {
       md->state[i] = md->state[i] + S[i];
   }

   return PKA_OK;
}


int pka_sha512_process (const uint8_t *in, uint32_t inlen, sha512_state *md)
{

   uint32_t n;
   int      err = PKA_OK;


   if (in == NULL || md == NULL) {
      return PKA_SHA_ERR;
   }
   if (md->curlen > sizeof(md->buf)) {
      return PKA_SHA_ERR;
   }

   while (inlen > 0) {
      if (md->curlen == 0 && inlen >= SHA_512_BLOCK_SIZE) {
           err = pka_sha512_compress (md, (uint8_t *)in);

           if (err != PKA_OK) {
              return err;
           }

           md->length += SHA_512_BLOCK_SIZE * 8;
           in             += SHA_512_BLOCK_SIZE;
           inlen          -= SHA_512_BLOCK_SIZE;
       } else {
           n = MIN(inlen, (SHA_512_BLOCK_SIZE - md->curlen));
           memcpy(md->buf + md->curlen, in, (size_t)n);
           md->curlen += n;
           in             += n;
           inlen          -= n;
           if (md->curlen == SHA_512_BLOCK_SIZE) {
              err = pka_sha512_compress (md, md->buf);

              if (err != PKA_OK) {
                 return err;
              }

              md->length += 8*SHA_512_BLOCK_SIZE;
              md->curlen = 0;
          }
       }
   }
   return err;
}


int pka_sha512_done(uint8_t *out, uint32_t *outlen, sha512_state *md)
{
   int i;

   if (out == NULL || outlen == NULL || md == NULL) {
      PKA_REPORT("Invalid pointer");
      return PKA_SHA_ERR;
   }

   if (md->curlen >= sizeof(md->buf)) {
      PKA_REPORT("Invalid SHA512 state");
      PKA_REPORT_ERR2("curlen, buf", md->curlen, sizeof(md->buf));
      return PKA_SHA_ERR;
   }

   if (*outlen < SHA_512_SIZE) {
      PKA_REPORT_ERR("Buffer overflow outlen", *outlen);
      *outlen = SHA_512_SIZE;
      return PKA_SHA_ERR;
   }

   *outlen = SHA_512_SIZE;

   /* increase the length of the message */
   md->length += md->curlen * CONST64(8);

   /* append the '1' bit */
   md->buf[md->curlen++] = (uint8_t)0x80;

   /* if the length is currently above 112 bytes we append zeros
    * then compress.  Then we can fall back to padding zeros and length
    * encoding like normal.
    */
   if (md->curlen > 112) {
      while (md->curlen < 128) {
         md->buf [md->curlen++] = (uint8_t)0;
      }
      pka_sha512_compress(md, md->buf);
      md->curlen = 0;
   }

   /* pad upto 120 bytes of zeroes
    * note: that from 112 to 120 is the 64 MSB of the length.  We assume that you won't hash
    * > 2^64 bits of data... :-)
    */
   while (md->curlen < 120) {
      md->buf[md->curlen++] = (uint8_t)0;
   }

   /* store length */
   STORE64H(md->length, md->buf+120);
   pka_sha512_compress(md, md->buf);

   /* copy output */
   for (i = 0; i < 8; i++) {
      STORE64H(md->state[i], out+(8*i));
   }
   return PKA_OK;
}

int pka_sha512(uint8_t *input, uint32_t input_size, uint8_t *hash)
{
   int err = PKA_SHA_ERR;
   uint32_t hash_size = SHA_512_SIZE;
   sha512_state shaState;

   err = pka_sha512_init(&shaState);

   if (err > 0)
   {
      PKA_REPORT_ERR("pka_sha512_init", err);
      return err;
   }

   err = pka_sha512_process(input, input_size, &shaState);

   if (err > 0)
   {
      PKA_REPORT_ERR("pka_sha512_process failed", err);
      return err;
   }

   err = pka_sha512_done(hash, &hash_size, &shaState);

   if (err > 0)
   {
      PKA_REPORT_ERR("pka_sha512_done", err);
      return err;
   }

   return err;
}

/* Internal HMAC algorithm  doing the following operation
   o_key_pad = key xor [0x5c * blockSize]   //Outer padded key
   i_key_pad = key xor [0x36 * blockSize]   //Inner padded key
   return hash(o_key_pad || i_hash(i_key_pad ||  message)) //Where || is concatenation
*/
int pka_hmac(uint8_t *key, uint8_t *msg, uint32_t mlen, uint8_t *out)
{
   int err = PKA_ERR;
   // HMAC
   uint8_t pad[SHA_512_BLOCK_SIZE];
   uint8_t hash_tmp[SHA_512_SIZE];
   uint32_t len = SHA_512_SIZE;
   sha512_state shaState;
   int i = 0;

   // Inner hash
   err = pka_sha512_init(&shaState);

   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_init", err);
   }

   for (i = 0; i < SHA_512_SIZE; i++)
   {
      pad[i] = key[i] ^ 0x36;
   }
   memset(&pad[i], 0x36, SHA_512_BLOCK_SIZE - SHA_512_SIZE);

   err = pka_sha512_process(pad, SHA_512_BLOCK_SIZE, &shaState);

   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_process", err);
   }

   err = pka_sha512_process(msg, mlen, &shaState);

   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_process", err);
   }

   err = pka_sha512_done(hash_tmp, &len, &shaState);

   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_done", err);
   }

   // Outer hash
   err = pka_sha512_init(&shaState);

   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_init", err);
   }

   for (i = 0; i < SHA_512_SIZE; i++)
   {
      pad[i] = key[i] ^ 0x5c;
   }
   memset(&pad[i], 0x5c, SHA_512_BLOCK_SIZE - SHA_512_SIZE);
   err = pka_sha512_process(pad, SHA_512_BLOCK_SIZE, &shaState);

   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_process", err);
   }

   err = pka_sha512_process(hash_tmp, SHA_512_SIZE, &shaState);

   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_process", err);
   }

   len = SHA_512_SIZE;
   err = pka_sha512_done(out, &len, &shaState);

   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_done", err);
   }

   return err;
}

