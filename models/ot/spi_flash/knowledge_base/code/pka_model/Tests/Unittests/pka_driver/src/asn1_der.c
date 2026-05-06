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
 * Copyright (c) 2019-2020, 2022 Synopsys, Inc. and/or its affiliates.
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

#include "asn1_der.h"

/* Based on the implementation of CSL elpder_small_encode and
 * elpder_small_decode for handling only 2 integers like 'r' and 's'
 * of an ECC and SM2 signature */

uint32_t count_bits(uint8_t *val, uint32_t size)
{
   uint32_t bitsize;
   uint32_t cnt;
   uint8_t byte;

   cnt = 0;
   bitsize = size * 8;

   while (val[cnt] == 0)
   {
      bitsize -= 8;
      cnt++;
   }

   byte = val[cnt];

   while (!(byte & 0x80))
   {
      byte <<= 1;
      bitsize -= 1;
   }

   return bitsize;
}

int pka_der_encode(uint8_t *out, uint32_t *outlen, uint32_t size, uint8_t *r, uint8_t *s)
{
   uint32_t x, y, z, outoffset, iszero, msb, bytes;
   uint8_t *numbers[2];

   if (out == 0 || outlen == 0 || r == 0 || s == 0)
   {
      PKA_REPORT_ERR("Invalid operands", PKA_INVPARAM);
      return PKA_INVPARAM;
   }

   if (*outlen < 16)
   {
      PKA_REPORT_ERR("Invalid outlen", PKA_INVPARAM);
      return PKA_INVPARAM;
   }

   numbers[0] = r;
   numbers[1] = s;

   /* start encoding at byte 4 */
   outoffset = 4;

   /* first count the size of the DER encoding of all of the integers */
   for (x = 0; x < 2; x++)
   {
      /* first we assume they are >= 0 so lets just see if the msb is set */
      y = count_bits(numbers[x], size);
      bytes = (y / 8 + ((y & 7) != 0 ? 1 : 0));
      z = 0;
      iszero = 0;
      msb = 0;

      if (y == 0 || ((y & 7) == 0))
      {
         /* the length is of the form 8k+7 which means that 2^7 * 2^8k is set [e.g. a msb in a byte] */
         ++z;
         if (y == 0)
         {
            iszero = 1;
         }
         else
         {
            msb = 1;
         }
      }

      /* now count the bytes */
      z += bytes;

      /* now figure out the length */
      if ((outoffset + 4 + z) >= *outlen)
      {
         PKA_REPORT_ERR("Overflow", PKA_ERR);
         goto OVF;
      } /* to make the code smaller we always require at least 4 bytes per INTEGER */
      out[outoffset++] = 0x02;
      if (z < 128)
      {
      }
      else
      {
         if (z < 256)
         {
            out[outoffset++] = 0x81;
         }
         else
         {
            out[outoffset++] = 0x82;
            out[outoffset++] = (z >> 8) & 0xFF;
         }
      }
      out[outoffset++] = z & 0xFF;
      /* store num */
      if (msb || iszero)
      {
         out[outoffset++] = 0x00;
      }
      if (iszero == 0)
      {
         if (size < bytes)
         {
            PKA_REPORT_ERR("Invalid input", PKA_ERR);
            goto OVF;
         }
         else if (size > bytes)
         {
            /* possible PKA leading zeros are not copied */
            memset(out + outoffset + bytes, 0, size - bytes);
         }
         memcpy(out + outoffset, numbers[x] + (size - bytes), bytes);

         outoffset += z - msb;
      }
   }

   /* now we have encoded all of the INTEGERS into outoffset - 4 bytes */
   outoffset -= 4;

   /* now let's encode the SEQUENCE header */
   z = 0;
   out[z++] = 0x30;
   if (outoffset < 128)
   {
   }
   else
   {
      if (outoffset < 256)
      {
         out[z++] = 0x81;
      }
      else
      {
         out[z++] = 0x82;
         out[z++] = (outoffset >> 8) & 0xFF;
      }
   }
   out[z++] = outoffset & 0xFF;

   /* now we have encoded the header in z bytes */
   if (z != 4)
   {
      /* move data from out[4+x] to out[z+x] */
      for (x = 0; x < outoffset; x++)
      {
         out[z + x] = out[4 + x];
      }
   }

   *outlen = z + outoffset;
   return PKA_OK;
OVF:
   return PKA_ERR;
}

int pka_der_decode(uint8_t *in, uint32_t inlen, uint32_t size, uint8_t *r, uint8_t *s)
{
   uint32_t payload, itemlen, x, y, z, bits;
   uint8_t *numbers[2];

   if (in == 0 || inlen < 16 || r == 0 || s == 0)
   {
      PKA_REPORT_ERR("Invalid operands", PKA_INVPARAM);
      return PKA_INVPARAM;
   }

   numbers[0] = r;
   numbers[1] = s;

   /* decode header (by this point we assume we have at least 4 bytes) */
   x = 0;
   if (in[x++] != 0x30)
   {
      PKA_REPORT_ERR("Invalid input", PKA_ERR);
      goto ERR;
   }
   if (in[x] < 128)
   {
      payload = in[x++];
   }
   else
   {
      y = in[x++] - 0x80;
      if (y > 2)
      {
         PKA_REPORT_ERR("Invalid input", PKA_ERR);
         goto ERR;
      }
      payload = in[x++];
      if (y == 2)
      {
         payload = (payload << 8) + in[x++];
      }
   }

   /* now we know we have payload bytes of payload */
   if ((payload + x) > inlen)
   {
      PKA_REPORT_ERR("Invalid input", PKA_ERR);
      goto ERR;
   }

   /* normalize in so that payload is the length we can read from in[0...payload-1] */
   in += x;

   /* now decode integers */
   for (y = x = 0; y < payload && x < 2; x++)
   {
      if ((y + 1) >= payload || in[y++] != 0x02)
      {
         PKA_REPORT_ERR("Invalid input", PKA_ERR);
         goto ERR;
      }
      if (in[y] < 128)
      {
         itemlen = in[y++];
      }
      else
      {
         z = in[y++] - 0x80;
         if ((z > 2) || ((y + z) >= payload))
         {
            PKA_REPORT_ERR("Invalid input", PKA_ERR);
            goto ERR;
         }
         itemlen = in[y++];
         if (z == 2)
         {
            itemlen = (itemlen << 8) + in[y++];
         }
      }

      /* now we have the length of this item */
      if ((itemlen + y) > payload)
      {
         PKA_REPORT_ERR("Invalid input", PKA_ERR);
         goto ERR;
      }

      bits = count_bits(&in[y], itemlen);
      if(!(bits&0x7))
      {
         y++;
         itemlen--;
      }
      /* now read it in with adding possible leading zeros for the PKA */
      if (size < itemlen)
      {
         PKA_REPORT_ERR("Invalid input", PKA_ERR);
         goto ERR;
      }
      else if (size > itemlen)
      {
         memset(numbers[x], 0, size - itemlen);
      }
      memcpy(numbers[x] + (size - itemlen), &in[y], itemlen);

      y += itemlen;
   }

   return PKA_OK;
ERR:
   return PKA_ERR;
}
