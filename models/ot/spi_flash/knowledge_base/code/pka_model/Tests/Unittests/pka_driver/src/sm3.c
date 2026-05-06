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
 * Copyright (c) 2020,2022 Synopsys, Inc. and/or its affiliates.
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

/*   SM3 hash implementation   */

#include "sm3.h"
#include "data_macros.h"

int pka_sm3_init(sm3_state * md)
{
   if (md == NULL) {
      PKA_REPORT("Invalid pointer");
      return PKA_SM3_ERR;
   }
   md->curlen = 0;
   md->length[0] = 0;
   md->length[1] = 0;
   md->state[0] = 0x7380166f;
   md->state[1] = 0x4914b2b9;
   md->state[2] = 0x172442d7;
   md->state[3] = 0xda8a0600;
   md->state[4] = 0xa96f30bc;
   md->state[5] = 0x163138aa;
   md->state[6] = 0xe38dee4d;
   md->state[7] = 0xb0fb0e4e;
   return PKA_OK;
}


#define P0(x) ((x) ^ ROL((x), 9) ^ ROL((x), 17))
#define P1(x) ((x) ^ ROL((x), 15) ^ ROL((x), 23))
#define T1 0x79CC4519
#define T2 0x7a879d8a

#define FF0(a,b,c) (a ^ b ^ c)
#define GG0(a,b,c) (a ^ b ^ c)
#define FF1(a,b,c) ((a&b)|(a&c)|(b&c))
#define GG1(a,b,c) ((a&b)|(~a&c))

/* compress 512-bits */
int pka_sm3_compress(sm3_state * md, uint8_t *buf)
{
   uint32_t S[8], W[68], t0, t1, s0, s1;
   #define Wprime(x) (W[x] ^ W[x+4])
   int i;

   /* copy state into S */
   for (i = 0; i < 8; i++) {
      S[i] = md->state[i];
   }

   /* copy the state into 512-bits into W[0..15] */
   for (i = 0; i < 16; i++) {
      LOAD32H(W[i], buf + (4*i));
   }

   /* fill W[16..67] */
   for (i = 16; i < 68; i++) {
      W[i] = P1(W[i-16] ^ W[i-9] ^ ROL(W[i-3], 15)) ^ ROL(W[i-13], 7) ^ W[i-6];
   }

   /* Compress */
   for (i = 0; i < 64; i++) {
      s0 = ROL(S[0], 12) + S[4] + ROL(((i < 16) ? T1 : T2), (i&31)); s0 = ROL(s0, 7);
      s1 = s0 ^ ROL(S[0], 12);
      if (i < 16) {
         t0 = FF0(S[0],S[1],S[2]) + S[3] + s1 + Wprime(i);
         t1 = GG0(S[4],S[5],S[6]) + S[7] + s0 + W[i];
      } else {
         t0 = FF1(S[0],S[1],S[2]) + S[3] + s1 + Wprime(i);
         t1 = GG1(S[4],S[5],S[6]) + S[7] + s0 + W[i];
      }
      S[3] = S[2];
      S[2] = ROL(S[1], 9);
      S[1] = S[0];
      S[0] = t0;
      S[7] = S[6];
      S[6] = ROL(S[5], 19);
      S[5] = S[4];
      S[4] = P0(t1);
   }

   /* feedback */
   for (i = 0; i < 8; i++) {
      md->state[i] = md->state[i] ^ S[i];
   }
   return PKA_OK;
}

/* this version is for 32-bit only platforms */
//#define HASH_PROCESS32(pka_sm3_process, pka_sm3_compress, sm3, 64)

int pka_sm3_process (const uint8_t *in, uint32_t inlen, sm3_state *md)
{
   uint32_t n;
   int      err;
   uint32_t tmp;

   if (in == NULL || md == NULL) {
      PKA_REPORT("Invalid pointer");
      return PKA_SM3_ERR;
   }
   if (md->curlen > sizeof(md->buf)) {
      PKA_REPORT_ERR2("Invalid size curlen, sizeof buf",
              md->curlen, sizeof(md->curlen));
      return PKA_SM3_ERR;
   }
   while (inlen > 0) {
      if (md->curlen == 0 && inlen >= SM3_BLOCK_SIZE) {
         if ((err = pka_sm3_compress(md, (uint8_t *)in)) != PKA_OK) {
            PKA_REPORT_ERR("pka_sm3_compress", err);
            return err;
          }
          tmp = md->length[0];
          md->length[0] += SM3_BLOCK_SIZE * 8;
          if (md->length[0] < tmp) { md->length[1] += 1; }
          in             += SM3_BLOCK_SIZE;
          inlen          -= SM3_BLOCK_SIZE;
       } else {
            n = MIN(inlen, (SM3_BLOCK_SIZE - md->curlen));
            memcpy(md->buf + md->curlen, in, (size_t)n);
            md->curlen += n;
            in         += n;
            inlen      -= n;
            if (md->curlen == SM3_BLOCK_SIZE) {
               if ((err = pka_sm3_compress(md, md->buf)) != PKA_OK) {
                  PKA_REPORT_ERR("pka_sm3_compress", err);
                  return err;
               }
               tmp = md->length[0];
               md->length[0] += SM3_BLOCK_SIZE * 8;
               if (md->length[0] < tmp) { md->length[1] += 1; }
               md->curlen = 0;
            }
       }
   }
   return PKA_OK;
}

int pka_sm3_done(uint8_t *out, uint32_t *outlen, sm3_state *md)
{
   int i;
   uint32_t tmp;

   if (out == NULL || outlen == NULL || md == NULL) {
      PKA_REPORT("Invalid pointer");
      return PKA_SM3_ERR;
   }

   if (md->curlen >= sizeof(md->buf)) {
      PKA_REPORT("Invalid SM3 state");
      return PKA_SM3_ERR;
   }

   if (*outlen < 32) {
      *outlen = 32;
      PKA_REPORT("Buffer overflow");
      return PKA_SM3_ERR;
   }
   *outlen = 32;

   /* increase the length of the message */
   tmp = md->length[0];
   md->length[0] += md->curlen * 8;
   if (md->length[0] < tmp) { md->length[1] += 1; }

   /* append the '1' bit */
   md->buf[md->curlen++] = (uint8_t)0x80;

   /* if the length is currently above 56 bytes we append zeros
    * then compress.  Then we can fall back to padding zeros and length
    * encoding like normal.
    */
   if (md->curlen > 56) {
      while (md->curlen < 64) {
         md->buf[md->curlen++] = (uint8_t)0;
      }
      pka_sm3_compress(md, md->buf);
      md->curlen = 0;
   }

   /* pad upto 56 bytes of zeroes */
   while (md->curlen < 56) {
      md->buf[md->curlen++] = (uint8_t)0;
   }

   /* store length */
   STORE32H(md->length[1], md->buf+56);
   STORE32H(md->length[0], md->buf+60);
   pka_sm3_compress(md, md->buf);

   /* copy output */
   for (i = 0; i < 8; i++) {
      STORE32H(md->state[i], out+(4*i));
   }
   return PKA_OK;
}

int pka_sm3(uint8_t *input, uint32_t input_size, uint8_t *hash)
{
   int err = PKA_SM3_ERR;
   uint32_t hash_size = SM3_SIZE;
   sm3_state sm3State;

   err = pka_sm3_init(&sm3State);

   if (err > 0)
   {
      PKA_REPORT_ERR("pka_sm3_init", err);
      return err;
   }

   err = pka_sm3_process(input, input_size, &sm3State);

   if (err > 0)
   {
      PKA_REPORT_ERR("pka_sm3_process failed", err);
      return err;
   }

   err = pka_sm3_done(hash, &hash_size, &sm3State);

   if (err > 0)
   {
      PKA_REPORT_ERR("pka_sm3_done", err);
      return err;
   }

   return err;
}
