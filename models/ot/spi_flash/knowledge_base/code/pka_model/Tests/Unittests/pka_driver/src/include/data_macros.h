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
 * Copyright (c) 2022 Synopsys, Inc. and/or its affiliates.
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

#ifndef DATA_MACROS_H_
#define DATA_MACROS_H_

// 32 bit endian swap
#define SWAP32(__x__) ( ((__x__) & 0xff) << 24 | ((__x__) & 0xff00) << 8 | ((__x__) & 0xff0000) >> 8 | (((__x__) >> 24) & 0xff))

#define MIN(x, y) ( ((x)<(y))?(x):(y) )

#define LOAD64H(x, y)                                                      \
   { x = (((uint64_t)((y)[0] & 255))<<56)|(((uint64_t)((y)[1] & 255))<<48) | \
         (((uint64_t)((y)[2] & 255))<<40)|(((uint64_t)((y)[3] & 255))<<32) | \
         (((uint64_t)((y)[4] & 255))<<24)|(((uint64_t)((y)[5] & 255))<<16) | \
         (((uint64_t)((y)[6] & 255))<<8)|(((uint64_t)((y)[7] & 255))); }

#define ROR64c(x, y) \
    ( ((((x)&CONST64(0xFFFFFFFFFFFFFFFF))>>((uint64_t)(y)&CONST64(63))) | \
      ((x)<<((uint64_t)(64-((y)&CONST64(63)))))) & CONST64(0xFFFFFFFFFFFFFFFF))

#define STORE64H(x, y)                                                         \
   { (y)[0] = (uint8_t)(((x)>>56)&255); (y)[1] = (uint8_t)(((x)>>48)&255);     \
     (y)[2] = (uint8_t)(((x)>>40)&255); (y)[3] = (uint8_t)(((x)>>32)&255);     \
     (y)[4] = (uint8_t)(((x)>>24)&255); (y)[5] = (uint8_t)(((x)>>16)&255);     \
     (y)[6] = (uint8_t)(((x)>>8)&255); (y)[7] = (uint8_t)((x)&255); }

#define LOAD32H(x, y)                       \
     { x = ((uint32_t)((y)[0] & 255)<<24) | \
           ((uint32_t)((y)[1] & 255)<<16) | \
           ((uint32_t)((y)[2] & 255)<<8)  | \
           ((uint32_t)((y)[3] & 255)); }

#define STORE32H(x, y)                                                           \
        { (y)[0] = (uint8_t)(((x)>>24)&255); (y)[1] = (uint8_t)(((x)>>16)&255);   \
          (y)[2] = (uint8_t)(((x)>>8)&255); (y)[3] = (uint8_t)((x)&255); }

#define ROL(x, y) ( (((uint32_t)(x)<<(uint32_t)((y)&31)) | (((uint32_t)(x)&0xFFFFFFFFUL)>>(uint32_t)(-y&31))) & 0xFFFFFFFFUL)

#endif /* _DATA_MACROS_H_ */
