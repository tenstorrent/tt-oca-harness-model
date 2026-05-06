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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "pka.h"
#include "pka_core.h"
#include "pka_core_ecc.h"
#include "sha512.h"
#include "asn1_der.h"
#include "sm3.h"
#include "data_macros.h"

// PKA_SW_CURVE_NIST_P256
static const uint8_t nist_m32[]     = {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static const uint8_t nist_a32[]     = {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC, };
static const uint8_t nist_b32[]     = {0x5A, 0xC6, 0x35, 0xD8, 0xAA, 0x3A, 0x93, 0xE7, 0xB3, 0xEB, 0xBD, 0x55, 0x76, 0x98, 0x86, 0xBC, 0x65, 0x1D, 0x06, 0xB0, 0xCC, 0x53, 0xB0, 0xF6, 0x3B, 0xCE, 0x3C, 0x3E, 0x27, 0xD2, 0x60, 0x4B, };
static const uint8_t nist_x32[]     = {0x6B, 0x17, 0xD1, 0xF2, 0xE1, 0x2C, 0x42, 0x47, 0xF8, 0xBC, 0xE6, 0xE5, 0x63, 0xA4, 0x40, 0xF2, 0x77, 0x03, 0x7D, 0x81, 0x2D, 0xEB, 0x33, 0xA0, 0xF4, 0xA1, 0x39, 0x45, 0xD8, 0x98, 0xC2, 0x96, };
static const uint8_t nist_y32[]     = {0x4F, 0xE3, 0x42, 0xE2, 0xFE, 0x1A, 0x7F, 0x9B, 0x8E, 0xE7, 0xEB, 0x4A, 0x7C, 0x0F, 0x9E, 0x16, 0x2B, 0xCE, 0x33, 0x57, 0x6B, 0x31, 0x5E, 0xCE, 0xCB, 0xB6, 0x40, 0x68, 0x37, 0xBF, 0x51, 0xF5, };
static const uint8_t nist_mp32[]    = {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, };
static const uint8_t nist_r_sqr32[] = {0x00, 0x00, 0x00, 0x04, 0xFF, 0xFF, 0xFF, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFB, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, };
static const uint8_t nist_nr32[]    = {0x66, 0xE1, 0x2D, 0x94, 0xF3, 0xD9, 0x56, 0x20, 0x28, 0x45, 0xB2, 0x39, 0x2B, 0x6B, 0xEC, 0x59, 0x46, 0x99, 0x79, 0x9C, 0x49, 0xBD, 0x6F, 0xA6, 0x83, 0x24, 0x4C, 0x95, 0xBE, 0x79, 0xEE, 0xA2, };
static const uint8_t nist_np32[]    = {0x60, 0xD0, 0x66, 0x33, 0xA9, 0xD6, 0x28, 0x1C, 0x50, 0xFE, 0x77, 0xEC, 0xC5, 0x88, 0xC6, 0xF6, 0x48, 0xC9, 0x44, 0x08, 0x7D, 0x74, 0xD2, 0xE4, 0xCC, 0xD1, 0xC8, 0xAA, 0xEE, 0x00, 0xBC, 0x4F, };
static const uint8_t nist_n32[]     = {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xBC, 0xE6, 0xFA, 0xAD, 0xA7, 0x17, 0x9E, 0x84, 0xF3, 0xB9, 0xCA, 0xC2, 0xFC, 0x63, 0x25, 0x51, };

// PKA_SW_CURVE_NIST_P384
static const uint8_t nist_m48[]     = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, };
static const uint8_t nist_a48[]     = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFC, };
static const uint8_t nist_b48[]     = {0xB3, 0x31, 0x2F, 0xA7, 0xE2, 0x3E, 0xE7, 0xE4, 0x98, 0x8E, 0x05, 0x6B, 0xE3, 0xF8, 0x2D, 0x19, 0x18, 0x1D, 0x9C, 0x6E, 0xFE, 0x81, 0x41, 0x12, 0x03, 0x14, 0x08, 0x8F, 0x50, 0x13, 0x87, 0x5A, 0xC6, 0x56, 0x39, 0x8D, 0x8A, 0x2E, 0xD1, 0x9D, 0x2A, 0x85, 0xC8, 0xED, 0xD3, 0xEC, 0x2A, 0xEF, };
static const uint8_t nist_x48[]     = {0xAA, 0x87, 0xCA, 0x22, 0xBE, 0x8B, 0x05, 0x37, 0x8E, 0xB1, 0xC7, 0x1E, 0xF3, 0x20, 0xAD, 0x74, 0x6E, 0x1D, 0x3B, 0x62, 0x8B, 0xA7, 0x9B, 0x98, 0x59, 0xF7, 0x41, 0xE0, 0x82, 0x54, 0x2A, 0x38, 0x55, 0x02, 0xF2, 0x5D, 0xBF, 0x55, 0x29, 0x6C, 0x3A, 0x54, 0x5E, 0x38, 0x72, 0x76, 0x0A, 0xB7, };
static const uint8_t nist_y48[]     = {0x36, 0x17, 0xDE, 0x4A, 0x96, 0x26, 0x2C, 0x6F, 0x5D, 0x9E, 0x98, 0xBF, 0x92, 0x92, 0xDC, 0x29, 0xF8, 0xF4, 0x1D, 0xBD, 0x28, 0x9A, 0x14, 0x7C, 0xE9, 0xDA, 0x31, 0x13, 0xB5, 0xF0, 0xB8, 0xC0, 0x0A, 0x60, 0xB1, 0xCE, 0x1D, 0x7E, 0x81, 0x9D, 0x7A, 0x43, 0x1D, 0x7C, 0x90, 0xEA, 0x0E, 0x5F, };
static const uint8_t nist_mp48[]    = {0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x02, 0xFF, 0xFF, 0xFF, 0xFC, 0xFF, 0xFF, 0xFF, 0xFA, 0xFF, 0xFF, 0xFF, 0xFB, 0xFF, 0xFF, 0xFF, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, };
static const uint8_t nist_r_sqr48[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFE, 0x00, 0x00, 0x00, 0x01, };
static const uint8_t nist_nr48[]    = {0x0C, 0x84, 0xEE, 0x01, 0x2B, 0x39, 0xBF, 0x21, 0x3F, 0xB0, 0x5B, 0x7A, 0x28, 0x26, 0x68, 0x95, 0xD4, 0x0D, 0x49, 0x17, 0x4A, 0xAB, 0x1C, 0xC5, 0xBC, 0x3E, 0x48, 0x3A, 0xFC, 0xB8, 0x29, 0x47, 0xFF, 0x3D, 0x81, 0xE5, 0xDF, 0x1A, 0xA4, 0x19, 0x2D, 0x31, 0x9B, 0x24, 0x19, 0xB4, 0x09, 0xA9, };
static const uint8_t nist_np48[]    = {0x35, 0x5C, 0xA8, 0x7D, 0xE3, 0x9D, 0xBB, 0x1F, 0xA1, 0x50, 0x20, 0x6C, 0xE4, 0xF1, 0x94, 0xAC, 0x78, 0xD4, 0xBA, 0x58, 0x66, 0xD6, 0x17, 0x87, 0xEE, 0x6C, 0x8E, 0x3D, 0xF4, 0x56, 0x24, 0xCE, 0x54, 0xA8, 0x85, 0x99, 0x5D, 0x20, 0xBB, 0x2B, 0x6E, 0xD4, 0x60, 0x89, 0xE8, 0x8F, 0xDC, 0x45, };
static const uint8_t nist_n48[]     = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC7, 0x63, 0x4D, 0x81, 0xF4, 0x37, 0x2D, 0xDF, 0x58, 0x1A, 0x0D, 0xB2, 0x48, 0xB0, 0xA7, 0x7A, 0xEC, 0xEC, 0x19, 0x6A, 0xCC, 0xC5, 0x29, 0x73, };

// PKA_SW_CURVE_NIST_P521
static const uint8_t nist_m66[]       = {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, };
static const uint8_t nist_a66[]       = {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC, };
static const uint8_t nist_b66[]       = {0x00, 0x51, 0x95, 0x3E, 0xB9, 0x61, 0x8E, 0x1C, 0x9A, 0x1F, 0x92, 0x9A, 0x21, 0xA0, 0xB6, 0x85, 0x40, 0xEE, 0xA2, 0xDA, 0x72, 0x5B, 0x99, 0xB3, 0x15, 0xF3, 0xB8, 0xB4, 0x89, 0x91, 0x8E, 0xF1, 0x09, 0xE1, 0x56, 0x19, 0x39, 0x51, 0xEC, 0x7E, 0x93, 0x7B, 0x16, 0x52, 0xC0, 0xBD, 0x3B, 0xB1, 0xBF, 0x07, 0x35, 0x73, 0xDF, 0x88, 0x3D, 0x2C, 0x34, 0xF1, 0xEF, 0x45, 0x1F, 0xD4, 0x6B, 0x50, 0x3F, 0x00, };
static const uint8_t nist_x66[]       = {0x00, 0xC6, 0x85, 0x8E, 0x06, 0xB7, 0x04, 0x04, 0xE9, 0xCD, 0x9E, 0x3E, 0xCB, 0x66, 0x23, 0x95, 0xB4, 0x42, 0x9C, 0x64, 0x81, 0x39, 0x05, 0x3F, 0xB5, 0x21, 0xF8, 0x28, 0xAF, 0x60, 0x6B, 0x4D, 0x3D, 0xBA, 0xA1, 0x4B, 0x5E, 0x77, 0xEF, 0xE7, 0x59, 0x28, 0xFE, 0x1D, 0xC1, 0x27, 0xA2, 0xFF, 0xA8, 0xDE, 0x33, 0x48, 0xB3, 0xC1, 0x85, 0x6A, 0x42, 0x9B, 0xF9, 0x7E, 0x7E, 0x31, 0xC2, 0xE5, 0xBD, 0x66, };
static const uint8_t nist_y66[]       = {0x01, 0x18, 0x39, 0x29, 0x6A, 0x78, 0x9A, 0x3B, 0xC0, 0x04, 0x5C, 0x8A, 0x5F, 0xB4, 0x2C, 0x7D, 0x1B, 0xD9, 0x98, 0xF5, 0x44, 0x49, 0x57, 0x9B, 0x44, 0x68, 0x17, 0xAF, 0xBD, 0x17, 0x27, 0x3E, 0x66, 0x2C, 0x97, 0xEE, 0x72, 0x99, 0x5E, 0xF4, 0x26, 0x40, 0xC5, 0x50, 0xB9, 0x01, 0x3F, 0xAD, 0x07, 0x61, 0x35, 0x3C, 0x70, 0x86, 0xA2, 0x72, 0xC2, 0x40, 0x88, 0xBE, 0x94, 0x76, 0x9F, 0xD1, 0x66, 0x50, };
static const uint8_t nist_mp66[66]    = {0};  // Not used
static const uint8_t nist_r_sqr66[66] = {0};  // Not used
static const uint8_t nist_nr66[]      = {0x00, 0x20, 0x47, 0xa8, 0x0e, 0x46, 0x8e, 0x69, 0x6d, 0x68, 0xeb, 0xfa, 0x31, 0x10, 0xe0, 0xf4, 0xb6, 0x38, 0x0f, 0x45, 0x24, 0xb4, 0x35, 0x15, 0x6f, 0x31, 0xb5, 0x86, 0xa3, 0x95, 0x9e, 0xf3, 0x3f, 0xce, 0xe9, 0x57, 0xb7, 0x0d, 0x56, 0x69, 0x36, 0xe6, 0x5b, 0x7a, 0xf2, 0xf3, 0x6f, 0x2b, 0x21, 0xd6, 0x1a, 0x3b, 0x8f, 0x1d, 0x34, 0xc4, 0x02, 0x8c, 0xe0, 0x6b, 0x3d, 0xda, 0x1d, 0x07, 0x08, 0x51, };
static const uint8_t nist_np66[]      = {0x00, 0xf2, 0xe4, 0xce, 0x0a, 0xdf, 0x19, 0x22, 0x01, 0x82, 0xde, 0x0e, 0x06, 0x03, 0x5e, 0xf8, 0xe9, 0x20, 0xbe, 0x81, 0xa6, 0x43, 0x10, 0x87, 0x5c, 0x38, 0x17, 0xdf, 0xcc, 0x6f, 0xff, 0x21, 0x62, 0xc6, 0x64, 0xee, 0xef, 0x4e, 0x4b, 0xc5, 0x47, 0x2a, 0x3a, 0xb0, 0xc1, 0xfd, 0x1d, 0x69, 0xd1, 0x1f, 0x1f, 0xe1, 0xbd, 0x05, 0x33, 0xfe, 0xef, 0x45, 0x1d, 0x2f, 0x5c, 0xcd, 0x79, 0xa9, 0x95, 0xc7, };
static const uint8_t nist_n66[]       = {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFA, 0x51, 0x86, 0x87, 0x83, 0xBF, 0x2F, 0x96, 0x6B, 0x7F, 0xCC, 0x01, 0x48, 0xF7, 0x09, 0xA5, 0xD0, 0x3B, 0xB5, 0xC9, 0xB8, 0x89, 0x9C, 0x47, 0xAE, 0xBB, 0x6F, 0xB7, 0x1E, 0x91, 0x38, 0x64, 0x09, };

// PKA_SW_CURVE_BRAINPOOL_P256R1
static const uint8_t bp_m32[]     = {0xA9, 0xFB, 0x57, 0xDB, 0xA1, 0xEE, 0xA9, 0xBC, 0x3E, 0x66, 0x0A, 0x90, 0x9D, 0x83, 0x8D, 0x72, 0x6E, 0x3B, 0xF6, 0x23, 0xD5, 0x26, 0x20, 0x28, 0x20, 0x13, 0x48, 0x1D, 0x1F, 0x6E, 0x53, 0x77, };
static const uint8_t bp_a32[]     = {0x7D, 0x5A, 0x09, 0x75, 0xFC, 0x2C, 0x30, 0x57, 0xEE, 0xF6, 0x75, 0x30, 0x41, 0x7A, 0xFF, 0xE7, 0xFB, 0x80, 0x55, 0xC1, 0x26, 0xDC, 0x5C, 0x6C, 0xE9, 0x4A, 0x4B, 0x44, 0xF3, 0x30, 0xB5, 0xD9, };
static const uint8_t bp_b32[]     = {0x26, 0xDC, 0x5C, 0x6C, 0xE9, 0x4A, 0x4B, 0x44, 0xF3, 0x30, 0xB5, 0xD9, 0xBB, 0xD7, 0x7C, 0xBF, 0x95, 0x84, 0x16, 0x29, 0x5C, 0xF7, 0xE1, 0xCE, 0x6B, 0xCC, 0xDC, 0x18, 0xFF, 0x8C, 0x07, 0xB6, };
static const uint8_t bp_x32[]     = {0x8B, 0xD2, 0xAE, 0xB9, 0xCB, 0x7E, 0x57, 0xCB, 0x2C, 0x4B, 0x48, 0x2F, 0xFC, 0x81, 0xB7, 0xAF, 0xB9, 0xDE, 0x27, 0xE1, 0xE3, 0xBD, 0x23, 0xC2, 0x3A, 0x44, 0x53, 0xBD, 0x9A, 0xCE, 0x32, 0x62, };
static const uint8_t bp_y32[]     = {0x54, 0x7E, 0xF8, 0x35, 0xC3, 0xDA, 0xC4, 0xFD, 0x97, 0xF8, 0x46, 0x1A, 0x14, 0x61, 0x1D, 0xC9, 0xC2, 0x77, 0x45, 0x13, 0x2D, 0xED, 0x8E, 0x54, 0x5C, 0x1D, 0x54, 0xC7, 0x2F, 0x04, 0x69, 0x97, };
static const uint8_t bp_mp32[]    = {0xDB, 0x43, 0xF8, 0xE6, 0xD0, 0xBD, 0x5A, 0xF5, 0xFD, 0xEC, 0xCE, 0xFB, 0x8A, 0xB1, 0xE8, 0x38, 0xDA, 0xB9, 0xE6, 0xA2, 0x27, 0x73, 0xCA, 0x1F, 0xC6, 0xA7, 0x55, 0x90, 0xCE, 0xFD, 0x89, 0xB9, };
static const uint8_t bp_r_sqr32[] = {0x47, 0x17, 0xAA, 0x21, 0xE5, 0x95, 0x7F, 0xA8, 0xA1, 0xEC, 0xDA, 0xCD, 0x6B, 0x1A, 0xC8, 0x07, 0x5C, 0xCE, 0x4C, 0x26, 0x61, 0x4D, 0x4F, 0x4D, 0x8C, 0xFE, 0xDF, 0x7B, 0xA6, 0x46, 0x5B, 0x6C, };
static const uint8_t bp_nr32[]    = {0x0B, 0x25, 0xF1, 0xB9, 0xC3, 0x23, 0x67, 0x62, 0x9B, 0x7F, 0x25, 0xE7, 0x6C, 0x81, 0x5C, 0xB0, 0xF3, 0x5D, 0x17, 0x6A, 0x11, 0x34, 0xE4, 0xA0, 0xE1, 0xD8, 0xD8, 0xDE, 0x33, 0x12, 0xFC, 0xA6, };
static const uint8_t bp_np32[]    = {0x38, 0x9E, 0x7E, 0xD4, 0x1F, 0xC5, 0x39, 0x0B, 0xA0, 0x1A, 0x1D, 0xF0, 0x69, 0x7D, 0x2B, 0x1E, 0x90, 0x37, 0x01, 0x2E, 0x53, 0x4C, 0x02, 0x55, 0xFB, 0xFF, 0xBE, 0xBD, 0xCB, 0xB4, 0x0E, 0xE9, };
static const uint8_t bp_n32[]     = {0xA9, 0xFB, 0x57, 0xDB, 0xA1, 0xEE, 0xA9, 0xBC, 0x3E, 0x66, 0x0A, 0x90, 0x9D, 0x83, 0x8D, 0x71, 0x8C, 0x39, 0x7A, 0xA3, 0xB5, 0x61, 0xA6, 0xF7, 0x90, 0x1E, 0x0E, 0x82, 0x97, 0x48, 0x56, 0xA7, };

// PKA_SW_CURVE_BRAINPOOL_P384R1
static const uint8_t bp_m48[]     = {0x8C, 0xB9, 0x1E, 0x82, 0xA3, 0x38, 0x6D, 0x28, 0x0F, 0x5D, 0x6F, 0x7E, 0x50, 0xE6, 0x41, 0xDF, 0x15, 0x2F, 0x71, 0x09, 0xED, 0x54, 0x56, 0xB4, 0x12, 0xB1, 0xDA, 0x19, 0x7F, 0xB7, 0x11, 0x23, 0xAC, 0xD3, 0xA7, 0x29, 0x90, 0x1D, 0x1A, 0x71, 0x87, 0x47, 0x00, 0x13, 0x31, 0x07, 0xEC, 0x53, };
static const uint8_t bp_a48[]     = {0x7B, 0xC3, 0x82, 0xC6, 0x3D, 0x8C, 0x15, 0x0C, 0x3C, 0x72, 0x08, 0x0A, 0xCE, 0x05, 0xAF, 0xA0, 0xC2, 0xBE, 0xA2, 0x8E, 0x4F, 0xB2, 0x27, 0x87, 0x13, 0x91, 0x65, 0xEF, 0xBA, 0x91, 0xF9, 0x0F, 0x8A, 0xA5, 0x81, 0x4A, 0x50, 0x3A, 0xD4, 0xEB, 0x04, 0xA8, 0xC7, 0xDD, 0x22, 0xCE, 0x28, 0x26, };
static const uint8_t bp_b48[]     = {0x04, 0xA8, 0xC7, 0xDD, 0x22, 0xCE, 0x28, 0x26, 0x8B, 0x39, 0xB5, 0x54, 0x16, 0xF0, 0x44, 0x7C, 0x2F, 0xB7, 0x7D, 0xE1, 0x07, 0xDC, 0xD2, 0xA6, 0x2E, 0x88, 0x0E, 0xA5, 0x3E, 0xEB, 0x62, 0xD5, 0x7C, 0xB4, 0x39, 0x02, 0x95, 0xDB, 0xC9, 0x94, 0x3A, 0xB7, 0x86, 0x96, 0xFA, 0x50, 0x4C, 0x11, };
static const uint8_t bp_x48[]     = {0x1D, 0x1C, 0x64, 0xF0, 0x68, 0xCF, 0x45, 0xFF, 0xA2, 0xA6, 0x3A, 0x81, 0xB7, 0xC1, 0x3F, 0x6B, 0x88, 0x47, 0xA3, 0xE7, 0x7E, 0xF1, 0x4F, 0xE3, 0xDB, 0x7F, 0xCA, 0xFE, 0x0C, 0xBD, 0x10, 0xE8, 0xE8, 0x26, 0xE0, 0x34, 0x36, 0xD6, 0x46, 0xAA, 0xEF, 0x87, 0xB2, 0xE2, 0x47, 0xD4, 0xAF, 0x1E, };
static const uint8_t bp_y48[]     = {0x8A, 0xBE, 0x1D, 0x75, 0x20, 0xF9, 0xC2, 0xA4, 0x5C, 0xB1, 0xEB, 0x8E, 0x95, 0xCF, 0xD5, 0x52, 0x62, 0xB7, 0x0B, 0x29, 0xFE, 0xEC, 0x58, 0x64, 0xE1, 0x9C, 0x05, 0x4F, 0xF9, 0x91, 0x29, 0x28, 0x0E, 0x46, 0x46, 0x21, 0x77, 0x91, 0x81, 0x11, 0x42, 0x82, 0x03, 0x41, 0x26, 0x3C, 0x53, 0x15, };
static const uint8_t bp_mp48[]    = {0xA3, 0xDC, 0x38, 0xE9, 0xBB, 0xC1, 0xCF, 0xEE, 0x0F, 0xAD, 0xD0, 0x2F, 0x4C, 0x6F, 0x86, 0xDF, 0xF5, 0xD5, 0x6C, 0x42, 0x19, 0xE5, 0xC2, 0x92, 0x1C, 0xBA, 0xE5, 0x87, 0x4D, 0x80, 0xFC, 0x16, 0x83, 0x4B, 0x1E, 0x52, 0xED, 0xF7, 0x75, 0x94, 0x9A, 0x6E, 0xA9, 0x6C, 0xEA, 0x9E, 0xC8, 0x25, };
static const uint8_t bp_r_sqr48[] = {0x36, 0xBF, 0x68, 0x83, 0x17, 0x8D, 0xF8, 0x42, 0xD5, 0xC6, 0xEF, 0x3B, 0xA5, 0x7E, 0x05, 0x2C, 0x62, 0x14, 0x01, 0x91, 0x99, 0x18, 0xD5, 0xAF, 0x8E, 0x28, 0xF9, 0x9C, 0xC9, 0x94, 0x08, 0x99, 0x53, 0x52, 0x83, 0x34, 0x3D, 0x7F, 0xD9, 0x65, 0x08, 0x7C, 0xEF, 0xFF, 0x40, 0xB6, 0x4B, 0xDE, };
static const uint8_t bp_nr48[]    = {0x0C, 0xE8, 0x94, 0x1A, 0x61, 0x4E, 0x97, 0xC2, 0x8F, 0x88, 0x6D, 0xC9, 0x65, 0x16, 0x5F, 0xDB, 0x57, 0x4A, 0x74, 0xCB, 0x52, 0xD7, 0x48, 0xFF, 0x2A, 0x92, 0x7E, 0x3B, 0x98, 0x02, 0x68, 0x8A, 0x37, 0x26, 0x4E, 0x20, 0x2F, 0x2B, 0x6B, 0x6E, 0xAC, 0x4E, 0xD3, 0xA2, 0xDE, 0x77, 0x1C, 0x8E, };
static const uint8_t bp_np48[]    = {0x60, 0xB1, 0x11, 0xF7, 0xBA, 0x0C, 0xF0, 0x21, 0xFF, 0x59, 0xAB, 0x33, 0xD1, 0x39, 0xCF, 0xDD, 0x30, 0x38, 0xC5, 0x5A, 0x2B, 0x36, 0x27, 0xDD, 0x4C, 0x65, 0xF2, 0x03, 0x46, 0x99, 0xB1, 0xCC, 0xC0, 0x1F, 0x33, 0x8D, 0xAA, 0x79, 0xFB, 0x39, 0x5C, 0xFE, 0xDD, 0x2A, 0x5C, 0xB5, 0xBB, 0x93, };
static const uint8_t bp_n48[]     = {0x8C, 0xB9, 0x1E, 0x82, 0xA3, 0x38, 0x6D, 0x28, 0x0F, 0x5D, 0x6F, 0x7E, 0x50, 0xE6, 0x41, 0xDF, 0x15, 0x2F, 0x71, 0x09, 0xED, 0x54, 0x56, 0xB3, 0x1F, 0x16, 0x6E, 0x6C, 0xAC, 0x04, 0x25, 0xA7, 0xCF, 0x3A, 0xB6, 0xAF, 0x6B, 0x7F, 0xC3, 0x10, 0x3B, 0x88, 0x32, 0x02, 0xE9, 0x04, 0x65, 0x65, };

// PKA_SW_CURVE_BRAINPOOL_P512R1
static const uint8_t bp_m64[]     = {0xAA, 0xDD, 0x9D, 0xB8, 0xDB, 0xE9, 0xC4, 0x8B, 0x3F, 0xD4, 0xE6, 0xAE, 0x33, 0xC9, 0xFC, 0x07, 0xCB, 0x30, 0x8D, 0xB3, 0xB3, 0xC9, 0xD2, 0x0E, 0xD6, 0x63, 0x9C, 0xCA, 0x70, 0x33, 0x08, 0x71, 0x7D, 0x4D, 0x9B, 0x00, 0x9B, 0xC6, 0x68, 0x42, 0xAE, 0xCD, 0xA1, 0x2A, 0xE6, 0xA3, 0x80, 0xE6, 0x28, 0x81, 0xFF, 0x2F, 0x2D, 0x82, 0xC6, 0x85, 0x28, 0xAA, 0x60, 0x56, 0x58, 0x3A, 0x48, 0xF3, };
static const uint8_t bp_a64[]     = {0x78, 0x30, 0xA3, 0x31, 0x8B, 0x60, 0x3B, 0x89, 0xE2, 0x32, 0x71, 0x45, 0xAC, 0x23, 0x4C, 0xC5, 0x94, 0xCB, 0xDD, 0x8D, 0x3D, 0xF9, 0x16, 0x10, 0xA8, 0x34, 0x41, 0xCA, 0xEA, 0x98, 0x63, 0xBC, 0x2D, 0xED, 0x5D, 0x5A, 0xA8, 0x25, 0x3A, 0xA1, 0x0A, 0x2E, 0xF1, 0xC9, 0x8B, 0x9A, 0xC8, 0xB5, 0x7F, 0x11, 0x17, 0xA7, 0x2B, 0xF2, 0xC7, 0xB9, 0xE7, 0xC1, 0xAC, 0x4D, 0x77, 0xFC, 0x94, 0xCA, };
static const uint8_t bp_b64[]     = {0x3D, 0xF9, 0x16, 0x10, 0xA8, 0x34, 0x41, 0xCA, 0xEA, 0x98, 0x63, 0xBC, 0x2D, 0xED, 0x5D, 0x5A, 0xA8, 0x25, 0x3A, 0xA1, 0x0A, 0x2E, 0xF1, 0xC9, 0x8B, 0x9A, 0xC8, 0xB5, 0x7F, 0x11, 0x17, 0xA7, 0x2B, 0xF2, 0xC7, 0xB9, 0xE7, 0xC1, 0xAC, 0x4D, 0x77, 0xFC, 0x94, 0xCA, 0xDC, 0x08, 0x3E, 0x67, 0x98, 0x40, 0x50, 0xB7, 0x5E, 0xBA, 0xE5, 0xDD, 0x28, 0x09, 0xBD, 0x63, 0x80, 0x16, 0xF7, 0x23, };
static const uint8_t bp_x64[]     = {0x81, 0xAE, 0xE4, 0xBD, 0xD8, 0x2E, 0xD9, 0x64, 0x5A, 0x21, 0x32, 0x2E, 0x9C, 0x4C, 0x6A, 0x93, 0x85, 0xED, 0x9F, 0x70, 0xB5, 0xD9, 0x16, 0xC1, 0xB4, 0x3B, 0x62, 0xEE, 0xF4, 0xD0, 0x09, 0x8E, 0xFF, 0x3B, 0x1F, 0x78, 0xE2, 0xD0, 0xD4, 0x8D, 0x50, 0xD1, 0x68, 0x7B, 0x93, 0xB9, 0x7D, 0x5F, 0x7C, 0x6D, 0x50, 0x47, 0x40, 0x6A, 0x5E, 0x68, 0x8B, 0x35, 0x22, 0x09, 0xBC, 0xB9, 0xF8, 0x22, };
static const uint8_t bp_y64[]     = {0x7D, 0xDE, 0x38, 0x5D, 0x56, 0x63, 0x32, 0xEC, 0xC0, 0xEA, 0xBF, 0xA9, 0xCF, 0x78, 0x22, 0xFD, 0xF2, 0x09, 0xF7, 0x00, 0x24, 0xA5, 0x7B, 0x1A, 0xA0, 0x00, 0xC5, 0x5B, 0x88, 0x1F, 0x81, 0x11, 0xB2, 0xDC, 0xDE, 0x49, 0x4A, 0x5F, 0x48, 0x5E, 0x5B, 0xCA, 0x4B, 0xD8, 0x8A, 0x27, 0x63, 0xAE, 0xD1, 0xCA, 0x2B, 0x2F, 0xA8, 0xF0, 0x54, 0x06, 0x78, 0xCD, 0x1E, 0x0F, 0x3A, 0xD8, 0x08, 0x92, };
static const uint8_t bp_mp64[]    = {0x4C, 0x39, 0x8E, 0x09, 0x22, 0x96, 0x3F, 0x73, 0xC8, 0x2B, 0x46, 0xFB, 0xFA, 0xAE, 0xA0, 0x06, 0x36, 0x82, 0xA0, 0x85, 0xD4, 0xF0, 0x55, 0x03, 0x16, 0x09, 0x8B, 0x3E, 0x75, 0xC2, 0x05, 0x72, 0xAE, 0xF7, 0x52, 0x8B, 0xC5, 0xA7, 0xE2, 0x43, 0x50, 0x65, 0x2E, 0xC0, 0x81, 0x5C, 0xFB, 0xCD, 0xED, 0xAA, 0x1D, 0x85, 0xA2, 0xCD, 0x1E, 0x7C, 0x83, 0x9B, 0x32, 0x20, 0x7D, 0x89, 0xEF, 0xC5, };
static const uint8_t bp_r_sqr64[] = {0x3C, 0x4C, 0x9D, 0x05, 0xA9, 0xFF, 0x64, 0x50, 0x20, 0x2E, 0x19, 0x40, 0x20, 0x56, 0xEE, 0xCC, 0xA1, 0x6D, 0xAA, 0x5F, 0xD4, 0x2B, 0xFF, 0x83, 0x19, 0x48, 0x6F, 0xD8, 0xD5, 0x89, 0x80, 0x57, 0xE0, 0xC1, 0x9A, 0x77, 0x83, 0x51, 0x4A, 0x25, 0x53, 0xB7, 0xF9, 0xBC, 0x90, 0x5A, 0xFF, 0xD3, 0x79, 0x3F, 0xB1, 0x30, 0x27, 0x15, 0x79, 0x05, 0x49, 0xAD, 0x14, 0x4A, 0x61, 0x58, 0xF2, 0x05, };
static const uint8_t bp_nr64[]    = {0xA7, 0x94, 0x58, 0x6A, 0x71, 0x84, 0x07, 0xB0, 0x95, 0xDF, 0x1B, 0x4C, 0x19, 0x4B, 0x2E, 0x56, 0x72, 0x3C, 0x37, 0xA2, 0x2F, 0x16, 0xBB, 0xDF, 0xD7, 0xF9, 0xCC, 0x26, 0x3B, 0x79, 0x0D, 0xE3, 0xA6, 0xF2, 0x30, 0xC7, 0x2F, 0x02, 0x07, 0xE8, 0x3E, 0xC6, 0x4B, 0xD0, 0x33, 0xB7, 0x62, 0x7F, 0x08, 0x86, 0xB7, 0x58, 0x95, 0x28, 0x3D, 0xDD, 0xD2, 0xA3, 0x68, 0x1E, 0xCD, 0xA8, 0x16, 0x71, };
static const uint8_t bp_np64[]    = {0x64, 0x3D, 0x19, 0xCB, 0x6C, 0x00, 0xFF, 0xE1, 0xC5, 0x2D, 0x8A, 0xE3, 0x37, 0x6C, 0x05, 0x92, 0x58, 0x6E, 0x46, 0xC8, 0xAA, 0xAA, 0x3A, 0x34, 0xE3, 0xF8, 0x62, 0x21, 0x16, 0x43, 0xD3, 0x63, 0x10, 0x5E, 0xD0, 0x4E, 0x96, 0x19, 0xC2, 0xD1, 0xC3, 0x8B, 0xBF, 0x48, 0x2B, 0xE2, 0xB4, 0x14, 0xC8, 0xE5, 0x38, 0x81, 0x28, 0xE5, 0xD3, 0x27, 0xAD, 0x49, 0x54, 0x1F, 0x0F, 0x1B, 0x70, 0x27, };
static const uint8_t bp_n64[]     = {0xAA, 0xDD, 0x9D, 0xB8, 0xDB, 0xE9, 0xC4, 0x8B, 0x3F, 0xD4, 0xE6, 0xAE, 0x33, 0xC9, 0xFC, 0x07, 0xCB, 0x30, 0x8D, 0xB3, 0xB3, 0xC9, 0xD2, 0x0E, 0xD6, 0x63, 0x9C, 0xCA, 0x70, 0x33, 0x08, 0x70, 0x55, 0x3E, 0x5C, 0x41, 0x4C, 0xA9, 0x26, 0x19, 0x41, 0x86, 0x61, 0x19, 0x7F, 0xAC, 0x10, 0x47, 0x1D, 0xB1, 0xD3, 0x81, 0x08, 0x5D, 0xDA, 0xDD, 0xB5, 0x87, 0x96, 0x82, 0x9C, 0xA9, 0x00, 0x69, };

// PKA_SW_CURVE_SM2_SCA256
/* sca256 curve parameters from GM/T 0003.5-2012 section 2 */
static const uint8_t sm2_m32[]    = { 0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static const uint8_t sm2_a32[]    = { 0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC };
static const uint8_t sm2_b32[]    = { 0x28, 0xE9, 0xFA, 0x9E, 0x9D, 0x9F, 0x5E, 0x34, 0x4D, 0x5A, 0x9E, 0x4B, 0xCF, 0x65, 0x09, 0xA7, 0xF3, 0x97, 0x89, 0xF5, 0x15, 0xAB, 0x8F, 0x92, 0xDD, 0xBC, 0xBD, 0x41, 0x4D, 0x94, 0x0E, 0x93 };
static const uint8_t sm2_x32[]    = { 0x32, 0xC4, 0xAE, 0x2C, 0x1F, 0x19, 0x81, 0x19, 0x5F, 0x99, 0x04, 0x46, 0x6A, 0x39, 0xC9, 0x94, 0x8F, 0xE3, 0x0B, 0xBF, 0xF2, 0x66, 0x0B, 0xE1, 0x71, 0x5A, 0x45, 0x89, 0x33, 0x4C, 0x74, 0xC7 };
static const uint8_t sm2_y32[]    = { 0xBC, 0x37, 0x36, 0xA2, 0xF4, 0xF6, 0x77, 0x9C, 0x59, 0xBD, 0xCE, 0xE3, 0x6B, 0x69, 0x21, 0x53, 0xD0, 0xA9, 0x87, 0x7C, 0xC6, 0x2A, 0x47, 0x40, 0x02, 0xDF, 0x32, 0xE5, 0x21, 0x39, 0xF0, 0xA0 };
static const uint8_t sm2_mp32[]   = { 0xFF, 0xFF, 0xFF, 0xFC, 0x00, 0x00, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFE, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
static const uint8_t sm2_r_sqr32[]= { 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03 };
static const uint8_t sm2_nr32[]   = { 0x1E, 0xB5, 0xE4, 0x12, 0xA2, 0x2B, 0x3D, 0x3B, 0x62, 0x0F, 0xC8, 0x4C, 0x3A, 0xFF, 0xE0, 0xD4, 0x34, 0x64, 0x50, 0x4A, 0xDE, 0x6F, 0xA2, 0xFA, 0x90, 0x11, 0x92, 0xAF, 0x7C, 0x11, 0x4F, 0x20 };
static const uint8_t sm2_np32[]   = { 0x6F, 0x39, 0x13, 0x2F, 0x82, 0xE4, 0xC7, 0xBC, 0x2B, 0x00, 0x68, 0xD3, 0xB0, 0x89, 0x41, 0xD4, 0xDF, 0x1E, 0x8D, 0x34, 0xFC, 0x83, 0x19, 0xA5, 0x32, 0x7F, 0x9E, 0x88, 0x72, 0x35, 0x09, 0x75 };
static const uint8_t sm2_n32[]    = { 0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x72, 0x03, 0xDF, 0x6B, 0x21, 0xC6, 0x05, 0x2B, 0x53, 0xBB, 0xF4, 0x09, 0x39, 0xD5, 0x41, 0x23 };


//
ecc_set_type ecc_sets[] = {
{
// PKA_SW_CURVE_NIST_P256
   32,
   256,
   nist_m32,
   nist_a32,
   nist_b32,
   nist_x32,
   nist_y32,
   nist_mp32,
   nist_r_sqr32,
   nist_nr32,
   nist_np32,
   nist_n32
},
// PKA_SW_CURVE_NIST_P384
{
   48,
   384,
   nist_m48,
   nist_a48,
   nist_b48,
   nist_x48,
   nist_y48,
   nist_mp48,
   nist_r_sqr48,
   nist_nr48,
   nist_np48,
   nist_n48
},


// PKA_SW_CURVE_NIST_P521
{
   66,
   528,
   nist_m66,
   nist_a66,
   nist_b66,
   nist_x66,
   nist_y66,
   nist_mp66,
   nist_r_sqr66,
   nist_nr66,
   nist_np66,
   nist_n66
},

// PKA_SW_CURVE_BRAINPOOL_P256R1
{
   32,
   256,
   bp_m32,
   bp_a32,
   bp_b32,
   bp_x32,
   bp_y32,
   bp_mp32,
   bp_r_sqr32,
   bp_nr32,
   bp_np32,
   bp_n32
},
// PKA_SW_CURVE_BRAINPOOL_P384R1
{
   48,
   384,
   bp_m48,
   bp_a48,
   bp_b48,
   bp_x48,
   bp_y48,
   bp_mp48,
   bp_r_sqr48,
   bp_nr48,
   bp_np48,
   bp_n48
},
// PKA_SW_CURVE_BRAINPOOL_P512R1
{
   64,
   512,
   bp_m64,
   bp_a64,
   bp_b64,
   bp_x64,
   bp_y64,
   bp_mp64,
   bp_r_sqr64,
   bp_nr64,
   bp_np64,
   bp_n64
},

// PKA_SW_CURVE_SM2_SCA256
{
   32,
   256,
   sm2_m32,
   sm2_a32,
   sm2_b32,
   sm2_x32,
   sm2_y32,
   sm2_mp32,
   sm2_r_sqr32,
   sm2_nr32,
   sm2_np32,
   sm2_n32
},
};

// ANS X9.63-2011 section 5.6.3
int pka_ecc_kdf(uint8_t *key, uint32_t keylen, uint32_t  cnt, uint8_t *out)
{
   int err = PKA_ERR;
   const uint8_t sha512_size = SHA_512_SIZE;
   uint32_t len = sha512_size;
   sha512_state shaState;
   uint8_t   counter[4]= {0,0,0,0};

   counter[0] = (cnt >> 24) & 0xff;
   counter[1] = (cnt >> 16) & 0xff;
   counter[2] = (cnt >> 8) & 0xff;
   counter[3] = cnt  & 0xff;
   err = pka_sha512_init(&shaState);

   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_init", err);
   }

   err = pka_sha512_process(key, keylen, &shaState);

   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_process", err);
   }

   err = pka_sha512_process(counter, 4, &shaState);

   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_process", err);
   }

   err = pka_sha512_done(out, &len, &shaState);

   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sha512_done", err);
   }

   return err;
}

// encrypt data length in bytes = 1 + (2*SIZE) + mlen + TAG_SIZE
int pka_ecc_encrypt(struct pka_state *state, enum eccCurve curve, struct ecc_point key, uint8_t *dprime, uint8_t *b, uint8_t *msg, uint32_t mlen, uint8_t *encdata)
{
   int err = PKA_ERR;
   struct ecc_point keypub;
   struct ecc_point key_secret;
   const uint8_t sha512_size = SHA_512_SIZE;
   uint8_t hash_h[sha512_size];
   uint8_t hash_tmp[sha512_size];
   uint32_t i = 0, j = 0;
   uint32_t  cnt;
   int CT_index = (2 *pka_curve_size[curve]) +1;

   keypub.x = calloc(1, pka_curve_size[curve]);
   keypub.y = calloc(1, pka_curve_size[curve]);
   key_secret.x = calloc(1, pka_curve_size[curve]);
   key_secret.y = calloc(1, pka_curve_size[curve]);

   encdata[0] = 0x4;
   err = pka_ecc_key_generate(state, curve, dprime, b, keypub);
   if (err !=  PKA_OK)
   {
      PKA_REPORT_ERR("pka_ecc_key_generate", err);
      goto EXIT_ENCRYPT;
   }
   memcpy(encdata +1  ,keypub.x , pka_curve_size[curve]);
   memcpy(encdata + 1+pka_curve_size[curve] ,keypub.y , pka_curve_size[curve]);

   err = pka_ecc_key_agreement(state, curve, key, dprime, b, key_secret);
   if (err !=  PKA_OK)
   {
      PKA_REPORT_ERR("pka_ecc_key_agreement", err);
      goto EXIT_ENCRYPT;
   }
   /* Use the KDF (Key Derivation Function) algorithm specified in X9.63-2011 5.6.3 with SHA-512 to generate key material
      and XOR it with the message input (M) to produce cipher text (CT),
      First handling the multiple of sha512_size without excess bytes */
   cnt = 1;
   for (i = 0; i< (mlen >> 6); i++)
   {
      err = pka_ecc_kdf(key_secret.x, pka_curve_size[curve], cnt, hash_h);
      if (err != PKA_OK)
      {
         PKA_REPORT_ERR("pka_ecc_kdf", err);
         goto EXIT_ENCRYPT;
      }

      for (j =0 ; j < sha512_size ; j++ )
      {
         encdata[CT_index + (i * sha512_size ) +j ] = hash_h[j] ^ msg [(i * sha512_size ) + j] ;
      }
      cnt ++;
   }

   // Handling the excess bytes in the MAC key ( if exists)  based on ANS X9.63-2011 section 5.8
   if (mlen & 0x3f)
   {
      err = pka_ecc_kdf(key_secret.x, pka_curve_size[curve], cnt, hash_h);
      if (err != PKA_OK)
      {
         PKA_REPORT_ERR("pka_ecc_kdf", err);
         goto EXIT_ENCRYPT;
      }
      for (j = 0; j <(mlen & 0x3f); j++)
      {
         encdata[CT_index + (i * sha512_size ) +j ] = hash_h[j] ^ msg [(i * sha512_size ) + j] ;
      }
      cnt++;
      //copy the remaining unused hash bytes , to be used for tag
      memcpy(hash_tmp , hash_h + (mlen & 0x3f), sha512_size -(mlen & 0x3f));
      err = pka_ecc_kdf(key_secret.x, pka_curve_size[curve], cnt, hash_h);
      if (err != PKA_OK)
      {
         PKA_REPORT_ERR("pka_ecc_kdf", err);
         goto EXIT_ENCRYPT;
      }

      memcpy(hash_tmp + (mlen & 0x3f), hash_h, sha512_size -(mlen & 0x3f));
      // The hash_tmp has the Mackey , copied to to hash_h to be consistent with the other cases
      memcpy(hash_h, hash_tmp, sha512_size);

   }
   else
   { // No excess bytes
      err = pka_ecc_kdf(key_secret.x, pka_curve_size[curve], cnt, hash_h);
      if (err != PKA_OK)
      {
         PKA_REPORT_ERR("pka_ecc_kdf", err);
         goto EXIT_ENCRYPT;
      }
   }

   err = pka_hmac(hash_h, encdata + CT_index, mlen, encdata + CT_index + mlen);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_hmac", err);
   }

EXIT_ENCRYPT:
   free(keypub.x);
   free(keypub.y);
   free(key_secret.x);
   free(key_secret.y);

   return err;
}

typedef struct pka_sm2_kdf_state {
    sm3_state hashstate;
    uint32_t ctr;
} pka_sm2_kdf_state;

/* initialize an sm2 kdf */
int pka_sm2_kdf_init(pka_sm2_kdf_state *kdf, const uint8_t *buf, uint32_t size)
{
   int err = PKA_OK;

   /* initialize counter */
   kdf->ctr = 0;

   /* initialize the hash, then hash in rpoint */
   err = pka_sm3_init(&kdf->hashstate);
   err = pka_sm3_process(buf, size, &kdf->hashstate);

   return err;
}

/* generate a block of sm2 kdf data */
int pka_sm2_kdf_getblock(pka_sm2_kdf_state *kdf, uint8_t *out, uint32_t *outlen)
{
   int err = PKA_ERR;

   sm3_state tmpstate;
   uint8_t   ctrbuf[4];
   uint32_t  i;

   /* copy the base hash state */
   tmpstate = kdf->hashstate;

   /* update the counter, then hash it */
   kdf->ctr++;
   STORE32H(kdf->ctr, ctrbuf);
   if ((err = pka_sm3_process(ctrbuf, sizeof(ctrbuf), &tmpstate)) != PKA_OK) { goto error; }
   if ((err = pka_sm3_done(out, outlen, &tmpstate)) != PKA_OK) { goto error; }

   /* if all zero, return error */
   err = PKA_ERR;
   for (i=0; i < *outlen; i++) {
      if (out[i] != 0) {
         err = PKA_OK;
         break;
      }
   }

error:
   return err;
}

// encrypt data length in bytes = 1 + 2*(PKA_CURVE_SM2_SCA256_BYTE) + SM3_SIZE + mlen
int pka_sm2_encrypt(struct pka_state *state, enum eccCurve curve, struct ecc_point pub_key, uint8_t *dprime, uint8_t *b, uint8_t *in, uint32_t inlen, uint8_t *out)
{
   int err = PKA_ERR;

   uint8_t size = pka_curve_size[curve];
   struct ecc_point base_point;
   uint8_t rpoint_x[size];
   uint8_t rpoint_y[size];
   struct ecc_point rpoint;
   uint8_t pointbuf[2 * size];
   uint8_t *n = (uint8_t *)ecc_sets[curve].n;    // curve order param
   uint8_t *a = (uint8_t *)ecc_sets[curve].a;    // curve a param
   uint8_t *m = (uint8_t *)ecc_sets[curve].m;    // curve order param
   uint8_t *mp = (uint8_t *)ecc_sets[curve].mp;  // curve mp param
   uint8_t *r_sqr = (uint8_t *)ecc_sets[curve].r_sqr;  // curve r_sqr param
   uint8_t k[size];     // random k
   const uint8_t *inp;  // pointer into input buffer
   uint8_t *outp;       // pointer into output buffer

   uint32_t msgbytes;
   uint8_t  i;
   pka_sm2_kdf_state kdfstate;
   uint8_t  dig[SM3_SIZE];
   uint32_t digsize;
   sm3_state hashstate; // current hash state

   rpoint.x = rpoint_x;
   rpoint.y = rpoint_y;
   base_point.x = (uint8_t *)ecc_sets[curve].x;
   base_point.y = (uint8_t *)ecc_sets[curve].y;

   // A1: make k in [1, n-1] using dprime input
   err = pka_core_ecc_mod(state, dprime, n, k, size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_mod", err);
      return err;
   }

   // A2: (x1, y1) = point mult (k * base_point) to rpoint
   err = pka_core_ecc_pmult(state, base_point, k, b, m, a, mp, r_sqr, size, rpoint);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_pmult", err);
   }

   outp = out; // outp points into the output buffer

   // A2: write uncompressed point to out (04 || x1 || y1)
   *outp = 0x04; // magic byte for uncompressed
   outp++;
   memcpy(outp, rpoint.x, size);
   outp += size;
   memcpy(outp, rpoint.y, size);
   outp += size;

   // A4: (x2, y2) = point mult (pub_key * k) reusing rpoint for output *
   err = pka_core_ecc_pmult(state, pub_key, k, b, m, a, mp, r_sqr, size, rpoint);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_pmult", err);
   }

   // A5/A6: loop over msg, generating bytes with KDF and writing xor output
   msgbytes = inlen; // init then count down
   inp = in;

   // SM3 HASH
   /* initialize kdf with (x2||y2) */
   memcpy(pointbuf, rpoint.x, size);
   memcpy(pointbuf + size, rpoint.y, size);
   pka_sm2_kdf_init(&kdfstate, pointbuf, 2*size);
   while (msgbytes > 0) {
      /* assumes this state copy works for the given hash */
      digsize = SM3_SIZE;
      err = pka_sm2_kdf_getblock(&kdfstate, dig, &digsize);
      if (err != PKA_OK)
      {
         // bail OUT if KDF gives all 0 for any block
         PKA_REPORT_ERR("pka_sm2_kdf_getblock", err);
         return err;
      }

      // xor one byte at a time until end of block or end of message
      for (i = 0; (msgbytes > 0) && (i < SM3_SIZE); i++) {
         outp[i] = inp[i] ^ dig[i];
         msgbytes--;
      }
      // move up the pointers
      outp += i;
      inp += i;
   }

   // A7: compute Hash(x2 || M || y2)
   digsize = SM3_SIZE;
   err = pka_sm3_init(&hashstate);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sm3_init", err);
   }
   err = pka_sm3_process(rpoint.x, SM3_SIZE, &hashstate);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sm3_process", err);
   }
   err = pka_sm3_process(in, inlen, &hashstate);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sm3_process", err);
   }
   err = pka_sm3_process(rpoint.y, SM3_SIZE, &hashstate);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sm3_process", err);
   }
   err = pka_sm3_done(outp, &digsize, &hashstate);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_sm3_done", err);
   }

   return err;
}

int pka_ecc_decrypt(struct pka_state *state, enum eccCurve curve, uint8_t *dprime, uint8_t *b, uint8_t *encdata, uint32_t encdatalen, uint8_t *outdata)
{
   int err = PKA_ERR;
   struct ecc_point keypub;
   struct ecc_point key_secret;
   const uint8_t sha512_size = SHA_512_SIZE;
   uint8_t hash_h[sha512_size];
   uint8_t hash_tmp[sha512_size];
   uint8_t tag[sha512_size];
   uint32_t i,j = 0;
   uint32_t  cnt, ctlen ;
   int CT_index = (2 *pka_curve_size[curve]) +1;


   keypub.x = calloc(1, pka_curve_size[curve]);
   keypub.y = calloc(1, pka_curve_size[curve]);
   key_secret.x = calloc(1, pka_curve_size[curve]);
   key_secret.y = calloc(1, pka_curve_size[curve]);

   memcpy(keypub.x, encdata +1 ,pka_curve_size[curve]);
   memcpy(keypub.y, encdata +1 + pka_curve_size[curve], pka_curve_size[curve]);

   err = pka_ecc_key_agreement(state, curve, keypub, dprime, b, key_secret);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_ecc_key_agreement", err);
      goto EXIT_DECRYPT;
   }

   ctlen = encdatalen -1 - (2 *pka_curve_size[curve])- TAG_SIZE;

    /* Use the KDF (Key Derivation Function) algorithm specified in X9.63-2011 5.6.3 with SHA-512 to generate key material
       and XOR it with the message input (M) to produce cipher text (CT),
       First handling the multiple of sha512_size */
    cnt = 1;
    for (i = 0; i< (ctlen >> 6); i++)
    {
       err = pka_ecc_kdf(key_secret.x, pka_curve_size[curve], cnt, hash_h);
       if (err != PKA_OK)
       {
          PKA_REPORT_ERR("pka_ecc_kdf", err);
          goto EXIT_DECRYPT;
       }

       for (j = 0; j < sha512_size; j++)
       {
          outdata[(i * sha512_size ) + j] = hash_h[j] ^ encdata[CT_index + (i * sha512_size ) +j] ;
       }
       cnt ++;
    }

    // Handling the excess bytes in the MAC key ( if exists)  based on ANS X9.63-2011 section 5.8
    if (ctlen & 0x3f)
    {
       err= pka_ecc_kdf(key_secret.x, pka_curve_size[curve], cnt, hash_h);
       if (err != PKA_OK)
       {
          PKA_REPORT_ERR("pka_ecc_kdf", err);
          goto EXIT_DECRYPT;
       }
       for (j=0 ; j <( ctlen & 0x3f); j++ )
       {
          outdata [(i * sha512_size ) + j] = hash_h[j] ^ encdata[CT_index + (i * sha512_size ) +j] ;
       }
       cnt ++;
       memcpy(hash_tmp, hash_h + (ctlen & 0x3f), sha512_size -(ctlen & 0x3f));
       err = pka_ecc_kdf(key_secret.x, pka_curve_size[curve], cnt, hash_h);
       if (err != PKA_OK)
       {
          PKA_REPORT_ERR("pka_ecc_kdf", err);
          goto EXIT_DECRYPT;
       }
       memcpy(hash_tmp + (ctlen & 0x3f) , hash_h  , sha512_size -(ctlen & 0x3f));
       // The hash_tmp has the Mackey , copied to to hash_h to be consistent with the other cases
       memcpy(hash_h  ,hash_tmp , sha512_size);
    }
    else
    {
       err = pka_ecc_kdf(key_secret.x, pka_curve_size[curve], cnt, hash_h);
       if (err != PKA_OK)
       {
         PKA_REPORT_ERR("pka_ecc_kdf", err);
         goto EXIT_DECRYPT;
       }
    }

    err = pka_hmac(hash_h, encdata + CT_index, ctlen, tag);
    if (err !=  PKA_OK)
    {
      PKA_REPORT_ERR("pka_hmac", err);
      goto EXIT_DECRYPT;
    }
    if (memcmp(tag, encdata + CT_index + ctlen , sha512_size) != 0)
    {
       PKA_REPORT("tag mismatch");
       err = PKA_VERFYFAIL;
    }

   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("Fail", err);
   }


EXIT_DECRYPT:
   free(keypub.x);
   free(keypub.y);
   free(key_secret.x);
   free(key_secret.y);

   return err;
}

int pka_sm2_decrypt(struct pka_state *state, enum eccCurve curve, uint8_t *dprime, uint8_t *b, uint8_t *encdata, uint32_t encdatalen, uint8_t *outdata)
{
   int err = PKA_ERR;
   uint8_t size = pka_curve_size[curve];
   uint8_t rpoint_x[size];
   uint8_t rpoint_y[size];
   struct ecc_point rpoint;
   uint8_t pointbuf[2 * size];

   uint8_t *a = (uint8_t *)ecc_sets[curve].a;    // a param
   uint8_t *m = (uint8_t *)ecc_sets[curve].m;    // modulus
   uint8_t *mp = (uint8_t *)ecc_sets[curve].mp;  // mp param
   uint8_t *r_sqr = (uint8_t *)ecc_sets[curve].r_sqr;   // r_sqr param
   uint8_t *outp;  // pointer into output buffer
   uint32_t msgbytes;
   uint8_t i;
   sm3_state hashstate;
   pka_sm2_kdf_state kdfstate;
   uint8_t dig[32];
   uint32_t digsize;
   uint32_t outdatalen;

   rpoint.x = rpoint_x;
   rpoint.y = rpoint_y;

   // B1: get C1 from C and convert to point, then verify point is on curve */
   if (encdata[0] != 4 && encdata[0] != 6 && encdata[0] != 7)
   {
      err = PKA_SM2_ERR;
      return err;
   }

   encdata++;
   memcpy(rpoint.x, encdata, size);
   encdata += size;
   memcpy(rpoint.y, encdata, size);
   encdata += size;

   // Verify rpoint is on curve
   err = pka_ecc_point_validate(state, curve, rpoint);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("Invalid Point", err);
      return err;
   }

   /* B3: compute (x2,y2) = [dB]C1 */
   err = pka_core_ecc_pmult(state, rpoint, dprime, b, m, a, mp, r_sqr, size, rpoint);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_pmult", err);
   }

   // B4/B5: loop over msg, generating bytes with KDF and writing xor output
   msgbytes = encdatalen - (2 * (size) + 1) - 32; //init then count down
   outp = outdata;

   /* initialize kdf with (x2||y2) */
   memcpy(pointbuf, rpoint.x, size);
   memcpy(pointbuf+size, rpoint.y, size);
   pka_sm2_kdf_init(&kdfstate, pointbuf, 2*size);

   while (msgbytes > 0) {
      /* assumes this state copy works for the given hash */
      digsize = SM3_SIZE;

      err = pka_sm2_kdf_getblock(&kdfstate, dig, &digsize);
      if (err != PKA_OK)
      {
         // bail OUT if KDF gives all 0 for any block
         PKA_REPORT_ERR("elpsm2_kdf_getblock", err);
         return err;
      }

      // xor one byte at a time until end of block or end of message
      for (i = 0; (msgbytes > 0) && (i < SM3_SIZE); i++) {
         outp[i] = encdata[i] ^ dig[i];
         msgbytes--;
      }
      // move up the pointers
      outp += i;
      encdata += i;
   }

   outdatalen = (uint32_t)(outp - outdata);

   // A7: compute Hash(x2 || M || y2)
    digsize = SM3_SIZE;
    pka_sm3_init(&hashstate);
    pka_sm3_process(rpoint.x, size, &hashstate);
    pka_sm3_process(outdata, outdatalen, &hashstate);
    pka_sm3_process(rpoint.y, size, &hashstate);
    pka_sm3_done(dig, &digsize, &hashstate);

   return err;
}

int pka_ecc_point_validate(struct pka_state *state, enum eccCurve curve, struct ecc_point key)
{
   int err = PKA_ERR;

   uint8_t *m     = (uint8_t *)ecc_sets[curve].m;
   uint8_t *a     = (uint8_t *)ecc_sets[curve].a;
   uint8_t *b     = (uint8_t *)ecc_sets[curve].b;
   uint8_t *mp    = (uint8_t *)ecc_sets[curve].mp;
   uint8_t *r_sqr = (uint8_t *)ecc_sets[curve].r_sqr;

   if (curve == PKA_SW_CURVE_NIST_P521)
   {
      err = pka_core_ecc_point_validate_521(state, m, a, b, pka_curve_size[curve], key);
   }
   else
   {
      err = pka_core_ecc_point_validate(state, m, a, b, mp, r_sqr, pka_curve_size[curve], key);
   }
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR(" ", err);
   }

   return err;
}

int pka_ecc_key_generate(struct pka_state *state, enum eccCurve curve, uint8_t *dprime, uint8_t *b, struct ecc_point keypub)
{
   int err = PKA_ERR;
   struct ecc_point curve_point;
   curve_point.x = (uint8_t *)ecc_sets[curve].x;
   curve_point.y = (uint8_t *)ecc_sets[curve].y;

   uint8_t *m = (uint8_t *)ecc_sets[curve].m;
   uint8_t *a = (uint8_t *)ecc_sets[curve].a;
   uint8_t *mp = (uint8_t *)ecc_sets[curve].mp;
   uint8_t *r_sqr = (uint8_t *)ecc_sets[curve].r_sqr;

   // guarantee no PKA_INVALID_RAND_NUM_B_RNG failure
   if (curve == PKA_SW_CURVE_NIST_P521)
   {
      b[0] = 0;
      dprime[0]&= 0x1;
      err = pka_core_ecc_pmult_521(state, curve_point, dprime, b, m, a, pka_curve_size[curve], keypub);
   }
   else
   {
      b[0] &= 0x7F;
      err = pka_core_ecc_pmult(state, curve_point, dprime, b, m, a, mp, r_sqr, pka_curve_size[curve], keypub);
   }

   if (err != PKA_OK)
   {
      PKA_REPORT_ERR(" ", err);
   }

   return err;
}

/* Prepare a user hash according to SM2 part 1 section 5.5.
 * uid_str   string containing UID
 * uidlen    length of UID string
 * pubkey    public key generated from private SM2 key
 * out       [out] destination for the user hash
 * outlen    [in/out] size of the out buffer and resulting hash size
 * return    PKA_OK if successful
*/
int pka_sm2_make_user_hash(const char *uid_str, uint16_t uidlen, struct ecc_point pubkey, uint8_t *out, uint32_t *outlen)
{
   int err = PKA_ERR;
   sm3_state  hashstate;
   uint8_t    entl[2];
   uint16_t   size = PKA_CURVE_SM2_SCA256_BYTE;

   uint8_t *a  = (uint8_t *)ecc_sets[PKA_SW_CURVE_SM2_SCA256].a;
   uint8_t *b  = (uint8_t *)ecc_sets[PKA_SW_CURVE_SM2_SCA256].b;
   uint8_t *x  = (uint8_t *)ecc_sets[PKA_SW_CURVE_SM2_SCA256].x;
   uint8_t *y  = (uint8_t *)ecc_sets[PKA_SW_CURVE_SM2_SCA256].y;

   if (uid_str == NULL || uidlen > 8191) {
      return PKA_INVRNGE;
   }

   /* we don't need to stop on any error here, so collect and handle at the end */
   /* initialize the hash */
   err = pka_sm3_init(&hashstate);

   /* add ENTL, 2 bytes representing length of UID string in bits - uidlen is in bytes */
   entl[0] = ((uidlen << 3) & 0xFF00) >> 8;
   entl[1] = (uidlen << 3) & 0xFF;
   err |= pka_sm3_process(entl, 2, &hashstate);

   /* add uid string */
   err |= pka_sm3_process((uint8_t*)uid_str, uidlen, &hashstate);

   /* add curve params, a, b, xG, yG */
   err |= pka_sm3_process(a, size, &hashstate);
   err |= pka_sm3_process(b, size, &hashstate);
   err |= pka_sm3_process(x, size, &hashstate);
   err |= pka_sm3_process(y, size, &hashstate);

   /* add pub key point, xA, yA */
   err |= pka_sm3_process(pubkey.x, size, &hashstate);
   err |= pka_sm3_process(pubkey.y, size, &hashstate);

   if (err == PKA_OK) {
      err = pka_sm3_done(out, outlen, &hashstate);
   }

   return err;
}

/** Prepare a message hash according to SM2 part 1 section 6.1, A1/A2.
 *
 */
int pka_sm2_make_msg_hash(const uint8_t *user_hash, uint32_t user_hash_len,
                       const uint8_t *msg, uint32_t msg_len,
                       uint8_t *out, uint32_t *outlen)
{
   sm3_state  sm3state;
   int        err = PKA_ERR;

   if (user_hash == NULL || msg == NULL ) {
      return PKA_SM3_ERR;
   }

   /* initialize the hash */
   if ((err = pka_sm3_init(&sm3state) != PKA_OK)) {goto error;}

   /* add user hash */
   if ((err = pka_sm3_process(user_hash, user_hash_len, &sm3state) != PKA_OK)) {goto error;}

   /* add message */
   if ((err = pka_sm3_process(msg, msg_len, &sm3state) != PKA_OK)) {goto error;}

   err = pka_sm3_done(out, outlen, &sm3state);

 error:
   return err;
}

int pka_sm2_key_generate(struct pka_state *state, enum eccCurve curve, uint8_t *dprime,
                         uint8_t *b, enum sm2_key_type keytype, struct ecc_point keypub)
{
   int err = PKA_ERR;
   struct ecc_point curve_point;
   uint16_t size = pka_curve_size[curve];
   curve_point.x = (uint8_t *)ecc_sets[curve].x;
   curve_point.y = (uint8_t *)ecc_sets[curve].y;

   uint8_t *m = (uint8_t *)ecc_sets[curve].m;
   uint8_t *a = (uint8_t *)ecc_sets[curve].a;
   uint8_t *mp = (uint8_t *)ecc_sets[curve].mp;
   uint8_t *r_sqr = (uint8_t *)ecc_sets[curve].r_sqr;

   if (curve != PKA_SW_CURVE_SM2_SCA256)
   {
      PKA_REPORT_ERR("Invalid Curve", curve);
      return err;
   }

   if (keytype == SM2_SIGNING_KEY)
   {
      uint8_t n[size];
      // dprime priv_key range [1, n-2]
      // sw check failure if dprime = n-1
      memcpy(n, (uint8_t *)ecc_sets[curve].n, size);
      n[31] &= 0xFE;
      if (memcmp(n, dprime, pka_curve_size[curve]) == 0)
      {
         PKA_REPORT_ERR("Invalid private key", PKA_INVRNGE);
         err = PKA_INVRNGE;
         return err;
      }
   }

   // guarantee no PKA_INVALID_RAND_NUM_B_RNG failure
   b[0] &= 0x7F;

   // public key generation
   err = pka_core_ecc_pmult(state, curve_point, dprime, b, m, a, mp, r_sqr, size, keypub);

   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_pmult", err);
   }

   return err;
}

/* ecc signature generation.
 *
 * state = PKA instance pointer.
 * curve = curve.
 * *key  = private key.
 * *b = blinding.
 * *k = second private ephemeral key.
 * *hash = hash.
 * hlen = hash length.
 * *r = signature r.
 * *s = signature s.
 */
int pka_ecc_sig_gen(struct pka_state *state, enum eccCurve curve, uint8_t *key, uint8_t *b, uint8_t *k, uint8_t *hash, uint32_t hlen, uint8_t *r, uint8_t *s)
{
   uint16_t size = pka_curve_size[curve];
   struct ecc_point k_pub;
   uint8_t k_pub_x[size];
   uint8_t k_pub_y[size];
   struct ecc_point curve_point;
   uint8_t *m = (uint8_t *)ecc_sets[curve].m;
   uint8_t *a = (uint8_t *)ecc_sets[curve].a;
   uint8_t *n = (uint8_t *)ecc_sets[curve].n;
   uint8_t *mp = (uint8_t *)ecc_sets[curve].mp;
   uint8_t *r_sqr = (uint8_t *)ecc_sets[curve].r_sqr;
   uint8_t *np = (uint8_t *)ecc_sets[curve].np;
   uint8_t *nr = (uint8_t *)ecc_sets[curve].nr;
   int err = PKA_ERR;

   k_pub.x = k_pub_x;
   k_pub.y = k_pub_y;
   curve_point.x = (uint8_t *)ecc_sets[curve].x;
   curve_point.y = (uint8_t *)ecc_sets[curve].y;

   // k is 2nd private key (ephemeral key)
   if (curve == PKA_SW_CURVE_NIST_P521)
   {
      err = pka_core_ecc_pmult_521(state, curve_point, k, b, m, a, size, k_pub);
      if (err != PKA_OK)
      {
         PKA_REPORT_ERR("pka_core_ecc_pmult_521", err);
         return err;
      }
   }
   else
   {
      err = pka_core_ecc_pmult(state, curve_point, k, b, m, a, mp, r_sqr, size, k_pub);
      if (err != PKA_OK)
      {
         PKA_REPORT_ERR("pka_core_ecc_pmult", err);
         return err;
      }
   }

   // Set r = j mod n
   err = pka_core_ecc_reduce(state, k_pub.x, n, r, size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_mod", err);
      return err;
   }

   err = pka_core_ecc_modmult(state, k_pub.x, key, n, np, nr, s, size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_modmult", err);
      return err;
   }

   err = pka_core_ecc_modadd(state, hash, hlen, s, n, s, size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_modadd", err);
      return err;
   }

   err = pka_core_ecc_moddiv(state, s,(uint32_t) size, k, n, s, size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_moddiv", err);
      return err;
   }

   return err;
}

/* ecc sm2 signature generation.
 *
 * state = PKA instance pointer.
 * curve = curve.
 * *key  = private key,
 * *k    = second private ephemeral key.
 * *b    = blinding.
 * *hash = hash.
 * hlen  = hash length.
 * *r    = signature r.
 * *s    = signature s.
 */
int pka_ecc_sm2_sig_gen(struct pka_state *state, enum eccCurve curve, uint8_t *key, uint8_t *b, uint8_t *k, uint8_t *hash, uint32_t hlen, uint8_t *r, uint8_t *s)
{
   // key is private key, k is ephemeral key
   uint16_t size = pka_curve_size[curve];
   struct ecc_point k_pub;
   uint8_t k_pub_x[size];
   uint8_t k_pub_y[size];
   struct ecc_point curve_point;
   uint8_t x[1];
   uint8_t res_0[PKA_CURVE_SM2_SCA256_BYTE];
   uint8_t res_1[PKA_CURVE_SM2_SCA256_BYTE];
   uint8_t *m = (uint8_t *)ecc_sets[curve].m;
   uint8_t *a = (uint8_t *)ecc_sets[curve].a;
   uint8_t *mp = (uint8_t *)ecc_sets[curve].mp;
   uint8_t *r_sqr = (uint8_t *)ecc_sets[curve].r_sqr;
   uint8_t *n = (uint8_t *)ecc_sets[curve].n;
   uint8_t *np = (uint8_t *)ecc_sets[curve].np;
   uint8_t *nr = (uint8_t *)ecc_sets[curve].nr;
   k_pub.x = k_pub_x;
   k_pub.y = k_pub_y;
   curve_point.x = (uint8_t *)ecc_sets[curve].x;
   curve_point.y = (uint8_t *)ecc_sets[curve].y;
   int err = PKA_ERR;

   if  (curve != PKA_SW_CURVE_SM2_SCA256)
   {
      PKA_REPORT_ERR("Invalid Curve", curve);
      return err;
   }

   // compute public point
   err = pka_core_ecc_pmult(state, curve_point, k, b, m, a, mp, r_sqr, size, k_pub);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_pmult", err);
      return err;
   }

   // r = (e + x1)
   err = pka_core_ecc_modadd(state, hash,  hlen, k_pub_x, n, r, size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_modadd", err);
      return err;
   }

   // 1 + DA (1 + private key)
   x[0] = 1;
   err = pka_core_ecc_modadd(state, x, 1, key, n, res_0, size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_modadd", err);
      return err;
   }

   // r * DA
   err = pka_core_ecc_modmult(state, r, key, n, np, nr, res_1, size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_modmult", err);
      return err;
   }

   // k - (r * DA)
   err = pka_core_ecc_modsub(state, k, res_1, n, res_1, size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_modsub", err);
      return err;
   }

   // s = ((k - (r * DA))/(1 + DA)) mod n
   err = pka_core_ecc_moddiv(state, res_1, size, res_0, n, s, size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_moddiv", err);
      return err;
   }

   return err;
}


/*
 * ecc signature verification.
 *
 * *state = PKA instance pointer.
 * curve  = curve.
 * keypub = public key ecc point.
 * *hash  = hash.
 * hlen   = hash length.
 * *r     = signature r.
 * *s     = signature s.
 */
int pka_ecc_sig_ver(struct pka_state *state, enum eccCurve curve, struct ecc_point keypub, uint8_t *hash, uint32_t hlen, uint8_t *r, uint8_t *s)
{
   int zerocheck, i, err = PKA_ERR;
   uint16_t size = pka_curve_size[curve];
   uint8_t  r2[size];
   struct ecc_point curve_point;
   uint8_t result_x[size];
   uint8_t result_y[size];
   struct ecc_point result;
   uint8_t tmp_result[size];

   uint8_t *m = (uint8_t *)ecc_sets[curve].m;
   uint8_t *a = (uint8_t *)ecc_sets[curve].a;
   uint8_t *mp = (uint8_t *)ecc_sets[curve].mp;
   uint8_t *r_sqr = (uint8_t *)ecc_sets[curve].r_sqr;
   uint8_t *n = (uint8_t *)ecc_sets[curve].n;
   curve_point.x = (uint8_t *)ecc_sets[curve].x;
   curve_point.y = (uint8_t *)ecc_sets[curve].y;

   result.x = result_x;
   result.y = result_y;

   // if r all zero or not in the interval [1, n – 1]  return error
   zerocheck = 0;
   for (i=0; i < size; i++) {
      zerocheck |= r[i];
      if (zerocheck != 0)
         break;
   }
   if (zerocheck == 0 || memcmp(r, n, size) >= 0)
   {
      PKA_REPORT_ERR("Invalid r", PKA_INVRNGE);
      return PKA_INVRNGE;
   }

   // if s all zero or not in the interval [1, n – 1]  return error
   zerocheck = 0;
   for (i=0; i < size; i++) {
      zerocheck |= s[i];
      if (zerocheck != 0)
         break;
   }
   if (zerocheck == 0 || memcmp(s, n, size) >= 0)
   {
      PKA_REPORT_ERR("Invalid s", PKA_INVRNGE);
      return PKA_INVRNGE;
   }

   err = pka_core_ecc_moddiv(state, r,(uint32_t) size, s, n, r2, size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_moddiv", err);
      return err;
   }

   err = pka_core_ecc_moddiv(state, hash, hlen, s, n, tmp_result, size );
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_moddiv", err);
      return err;
   }

   // Try Shamir's Trick first
   if (curve == PKA_SW_CURVE_NIST_P521)
   {
      err = pka_core_pmult2add_521(state, keypub, r2, curve_point, tmp_result, m, a, size, result);
   } else
   {
      err = pka_core_pmult2add(state, keypub, r2, curve_point, tmp_result, m, a, mp, r_sqr, size, result);
   }

   if (err != PKA_OK)
   {
      if (err == PKA_RC_REASON_INVALID_POINT)
      {
         // Try again without using Shamir
         uint8_t blind[size];
         memset(blind, 1, size);

         if (curve == PKA_SW_CURVE_NIST_P521)
         {
            err = pka_core_ecc_pmult_521(state, keypub, r2, blind, m, a, size, keypub);
            if (err != PKA_OK)
            {
               PKA_REPORT_ERR("pka_core_ecc_pmult_521", err);
               return err;
            }

            err = pka_core_ecc_pmult_521(state, curve_point, tmp_result, blind, m, a, size, result);
            if (err != PKA_OK)
            {
               PKA_REPORT_ERR("pka_core_ecc_pmult_521", err);
               return err;
            }
         } else
         {
            err = pka_core_ecc_pmult(state, keypub, r2, blind, m, a, mp, r_sqr, size, keypub);
            if (err != PKA_OK)
            {
               PKA_REPORT_ERR("pka_core_ecc_pmult", err);
               return err;
            }

            err = pka_core_ecc_pmult(state, curve_point, tmp_result, blind, m, a, mp, r_sqr, size, result);
            if (err != PKA_OK)
            {
               PKA_REPORT_ERR("pka_core_ecc_pmult", err);
               return err;
            }
         }

         if (curve == PKA_SW_CURVE_NIST_P521)
         {
            err = pka_core_ecc_point_add_521(state, result, keypub, m, a, size, result);
         }
         else
         {
            err = pka_core_ecc_point_add(state, result, keypub, m, a, mp, r_sqr, size, result);
         }
         if (err != PKA_OK)
         {
            PKA_REPORT_ERR(" ", err);
            return err;
         }
      }
      else
      {
         PKA_REPORT_ERR("Shamir fail", err);
         return err;
      }
   }

   //  Compute v = j mod n before comparing
   err = pka_core_ecc_reduce(state, result_x, n, result_x, size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_mod", err);
      return err;
   }
   if (memcmp(r, result_x, size))
   {
      err = PKA_VERFYFAIL;
   }
   else
   {
      err = PKA_OK;
   }

   return err;
}

/*
 * ecc sm2 signature verification.
 *
 * *state = PKA instance pointer.
 * curve  = curve.
 * keypub = public key ecc point.
 * *hash  = hash.
 * hlen   = hash length.
 * *r     = signature r.
 * *s     = signature s.
 */
int pka_ecc_sm2_sig_ver(struct pka_state *state, enum eccCurve curve, struct ecc_point keypub, uint8_t *hash, uint32_t hlen, uint8_t *r, uint8_t *s)
{
   int err = PKA_ERR;
   uint16_t size = pka_curve_size[curve];
   uint8_t *m = (uint8_t *)ecc_sets[curve].m;
   uint8_t *a = (uint8_t *)ecc_sets[curve].a;
   uint8_t *mp = (uint8_t *)ecc_sets[curve].mp;
   uint8_t *r_sqr = (uint8_t *)ecc_sets[curve].r_sqr;
   uint8_t *n = (uint8_t *)ecc_sets[curve].n;

   struct ecc_point curve_point;
   uint8_t result0_x[size];
   uint8_t result0_y[size];
   uint8_t result1_x[size];
   uint8_t result1_y[size];
   struct ecc_point result0;
   struct ecc_point result1;

   uint8_t t[PKA_CURVE_SM2_SCA256_BYTE] = {0};
   uint8_t blind[PKA_CURVE_SM2_SCA256_BYTE] = {1};

   result0.x = result0_x;
   result0.y = result0_y;
   result1.x = result1_x;
   result1.y = result1_y;

   // t = (r + s) mod n
   err = pka_core_ecc_modadd(state, r, size, s, n, t, size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_modadd", err);
      return err;
   }

   // Try Shamir's Trick first
   // [s]G + t[Pa]
   curve_point.x = (uint8_t *)ecc_sets[curve].x;
   curve_point.y = (uint8_t *)ecc_sets[curve].y;
   err = pka_core_pmult2add(state, curve_point, s, keypub, t, m, a, mp, r_sqr, size, result0);

   if (err != PKA_OK)
   {
      if (err == PKA_RC_REASON_INVALID_POINT)
      {
         // Try again without using Shamir
         // [s]G
         err = pka_core_ecc_pmult(state, curve_point, s, blind, m, a, mp, r_sqr, size, result0);
         if (err != PKA_OK)
         {
            PKA_REPORT_ERR("pka_core_ecc_pmult", err);
            return err;
         }

         // t[Pa]
         err = pka_core_ecc_pmult(state, keypub, t, blind, m, a, mp, r_sqr, size, result1);
         if (err != PKA_OK)
         {
            PKA_REPORT_ERR("pka_core_ecc_pmult", err);
            return err;
         }

         // [s]G + t[Pa]
         err = pka_core_ecc_point_add(state, result0, result1, m, a, mp, r_sqr, size, result0);
         if (err != PKA_OK)
         {
            PKA_REPORT_ERR("pka_core_ecc_point_add", err);
            return err;
         }
      }
      else
      {
         PKA_REPORT_ERR("Shamir fail", err);
         return err;
      }
   }

   // R = (e + (([s]G + [t]Pa x))) mod n
   err = pka_core_ecc_modadd(state, hash, hlen, result0.x, n, t, size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_modadd", err);
      return err;
   }

   // check
   if (memcmp(r, t, size))
   {
      err = PKA_VERFYFAIL;
   }
   else
   {
      err = PKA_OK;
   }

   return err;
}

int pka_ecc_sign_hash(struct pka_state *state, enum eccCurve curve, uint8_t *key, uint8_t *b, uint8_t *k, uint8_t *hash, uint32_t hlen, uint8_t *r, uint8_t *s)
{
   int err = PKA_ERR;
   uint32_t size = 0;

   // truncate hash length
   if (hlen > pka_curve_size[curve])
   {
      size = pka_curve_size[curve];
   }
   else
   {
      size = hlen;
   }

   // guarantee no PKA_INVALID_RAND_NUM_B_RNG failure
   if (curve == PKA_SW_CURVE_NIST_P521)
   {
      b[0] = 0;
      k[0]&= 0x1;
   }
   else
   {
      b[0] &= 0x7F;
   }

   err = pka_ecc_sig_gen(state, curve, key, b, k, hash, size, r, s);

   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_ecc_sig_gen", err);
   }

   return err;
}

int pka_sm2_sign_hash(struct pka_state *state, enum eccCurve curve, uint8_t *key, uint8_t *b, uint8_t *k, uint8_t *hash, uint32_t hlen, uint8_t *r, uint8_t *s)
{
   int err = PKA_ERR;
   uint32_t size = 0;

   // truncate hash length
   if (hlen > pka_curve_size[curve])
   {
      size = pka_curve_size[curve];
   }
   else
   {
      size = hlen;
   }

   // guarantee no PKA_INVALID_RAND_NUM_B_RNG failure
   if (curve == PKA_SW_CURVE_NIST_P521)
   {
      b[0] = 0;
      k[0]&= 0x1;
   }
   else
   {
      b[0] &= 0x7F;
   }

   err = pka_ecc_sm2_sig_gen(state, curve, key, b, k, hash, size, r, s);

   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_ecc_sm2_sig_gen", err);
   }

   return err;
}

int pka_ecc_sign_hash_der(struct pka_state *state, enum eccCurve curve, uint8_t *key, uint8_t *b, uint8_t *k, uint8_t *hash, uint32_t hlen, uint8_t * signature, uint32_t *sig_len)
{
   uint8_t *r;
   uint8_t *s;
   int err;

   err = PKA_ERR;
   r = malloc(pka_curve_size[curve]);
   s = malloc(pka_curve_size[curve]);

   err = pka_ecc_sign_hash(state, curve, key, b, k, hash, hlen, r, s);
   if (err == PKA_OK)
   {
      err = pka_der_encode(signature, sig_len, pka_curve_size[curve], r, s);

      if (err != PKA_OK)
      {
         PKA_REPORT_ERR("pka_der_encode", err);
      }
   }
   else
   {
      PKA_REPORT_ERR("pka_ecc_sign_hash", err);
   }

   free(r);
   free(s);

   return err;
}

int pka_sm2_sign_hash_der(struct pka_state *state, enum eccCurve curve, uint8_t *key, uint8_t *b, uint8_t *k, uint8_t *hash, uint32_t hlen, uint8_t * signature, uint32_t *sig_len)
{
   uint8_t *r;
   uint8_t *s;
   int err;

   err = PKA_ERR;
   r = malloc(pka_curve_size[curve]);
   s = malloc(pka_curve_size[curve]);

   err = pka_sm2_sign_hash(state, curve, key, b, k, hash, hlen, r, s);
   if (err == PKA_OK)
   {
      err = pka_der_encode(signature, sig_len, pka_curve_size[curve], r, s);

      if (err != PKA_OK)
      {
         PKA_REPORT_ERR("pka_der_encode", err);
      }
   }
   else
   {
      PKA_REPORT_ERR("pka_sm2_sign_hash_der", err);
   }

   free(r);
   free(s);

   return err;
}

int pka_ecc_verify_hash(struct pka_state *state, enum eccCurve curve, struct ecc_point keypub, uint8_t *hash, uint32_t hlen, uint8_t *r, uint8_t *s)
{
   int err = PKA_ERR;
   uint32_t size = 0;

   // truncate hash length
   if (hlen > pka_curve_size[curve])
   {
      size = pka_curve_size[curve];
   }
   else
   {
      size = hlen;
   }

   err = pka_ecc_sig_ver(state, curve, keypub, hash, size, r, s);

   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("ECC sig ver err ", err);
   }

   return err;
}

int pka_sm2_verify_hash(struct pka_state *state, enum eccCurve curve, struct ecc_point keypub, uint8_t *hash, uint32_t hlen, uint8_t *r, uint8_t *s)
{
   int err = PKA_ERR;
   uint32_t size = 0;

   // truncate hash length
   if (hlen > pka_curve_size[curve])
   {
      size = pka_curve_size[curve];
   }
   else
   {
      size = hlen;
   }

   err = pka_ecc_sm2_sig_ver(state, curve, keypub, hash, size, r, s);

   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_ecc_sm2_sig_ver", err);
   }

   return err;
}

int pka_ecc_verify_hash_der(struct pka_state *state, enum eccCurve curve, struct ecc_point keypub, uint8_t *hash, uint32_t hlen, uint8_t *signature, uint32_t sig_len)
{
   int err;
   uint8_t *r;
   uint8_t *s;

   err = PKA_ERR;
   r = malloc(pka_curve_size[curve]);
   s = malloc(pka_curve_size[curve]);

   err = pka_der_decode(signature, sig_len, pka_curve_size[curve], r, s);
   if (err == PKA_OK)
   {
      err = pka_ecc_verify_hash(state, curve, keypub, hash, hlen, r, s);

      if (err != PKA_OK)
      {
         PKA_REPORT_ERR("pka_ecc_verify_hash", err);
      }
   }
   else
   {
      PKA_REPORT_ERR("pka_der_decode", err);
   }

   free(r);
   free(s);
   return err;
}

int pka_sm2_verify_hash_der(struct pka_state *state, enum eccCurve curve, struct ecc_point keypub, uint8_t *hash, uint32_t hlen, uint8_t *signature, uint32_t sig_len)
{
   int err;
   uint8_t *r;
   uint8_t *s;

   err = PKA_ERR;
   r = malloc(pka_curve_size[curve]);
   s = malloc(pka_curve_size[curve]);

   err = pka_der_decode(signature, sig_len, pka_curve_size[curve], r, s);
   if (err == PKA_OK)
   {
      err = pka_sm2_verify_hash(state, curve, keypub, hash, hlen, r, s);

      if (err != PKA_OK)
      {
         PKA_REPORT_ERR("pka_sm2_verify_hash", err);
      }
   }
   else
   {
      PKA_REPORT_ERR("pka_der_decode", err);
   }

   free(r);
   free(s);
   return err;
}

int pka_ecc_key_agreement(struct pka_state *state, enum eccCurve curve, struct ecc_point keypub, uint8_t *keypriv, uint8_t *b, struct ecc_point secret)
{
   int err = PKA_ERR;
   uint8_t *m = (uint8_t *)ecc_sets[curve].m;
   uint8_t *a = (uint8_t *)ecc_sets[curve].a;
   uint8_t *mp = (uint8_t *)ecc_sets[curve].mp;
   uint8_t *r_sqr = (uint8_t *)ecc_sets[curve].r_sqr;

   // guarantee no PKA_INVALID_RAND_NUM_B_RNG failure
   if (curve == PKA_SW_CURVE_NIST_P521)
   {
      b[0] = 0;
      err = pka_core_ecc_pmult_521(state, keypub, keypriv, b, m, a, pka_curve_size[curve], secret);
   }
   else
   {
      b[0] &= 0x7F;
      err = pka_core_ecc_pmult(state, keypub, keypriv, b, m, a, mp, r_sqr, pka_curve_size[curve], secret);
   }

   if (err != PKA_OK)
   {
      PKA_REPORT_ERR(" ", err);
   }

   return err;
}

#define MAX_SM2_HASH_BLOCKSIZE 32
#define SM2_ZBUF_SIZE (2*PKA_CURVE_SM2_SCA256_BYTE + 2*MAX_SM2_HASH_BLOCKSIZE)

int pka_sm2_key_agreement(struct pka_state        *state,
                          enum eccCurve           curve,
                          struct ecc_point        far_key,
                          struct ecc_point        far_ephkey,
                          const uint8_t           *far_userhash,
                          uint8_t                 *my_key,
                          uint8_t                 *my_ephkey,
                          struct ecc_point        my_ephkey_pt,
                          const uint8_t           *my_userhash,
                          uint8_t                 *b,
                          enum sm2_key_agree_role role,
                          uint8_t                 *out,
                          uint32_t                outlen)
{
   int        err = PKA_ERR;
   int        size = pka_curve_size[curve];
   uint8_t    t[size];
   uint8_t    tmp[size];
   struct ecc_point u_point;
   uint8_t    u_point_x[size];
   uint8_t    u_point_y[size];
   uint8_t    zbuf[SM2_ZBUF_SIZE]; // input bit string for KDF
   uint8_t    *zbufp; // pointer into zbuf
   uint8_t    kdfblock[MAX_SM2_HASH_BLOCKSIZE];
   uint32_t   kdfsize;
   uint32_t   chunksize;
   pka_sm2_kdf_state kdfstate;

   /* read in the specs for this curve */
   uint8_t *m = (uint8_t *)ecc_sets[curve].m;
   uint8_t *a = (uint8_t *)ecc_sets[curve].a;
   uint8_t *mp = (uint8_t *)ecc_sets[curve].mp;
   uint8_t *r_sqr = (uint8_t *)ecc_sets[curve].r_sqr;
   uint8_t *n = (uint8_t *)ecc_sets[curve].n;
   uint8_t *np = (uint8_t *)ecc_sets[curve].np;
   uint8_t *nr = (uint8_t *)ecc_sets[curve].nr;

   if (far_userhash == NULL
       || my_key == NULL || my_ephkey == NULL || my_userhash == NULL
       || out == NULL) {
      PKA_REPORT_ERR("Invalid pointer", PKA_INVRNGE);
      return PKA_INVRNGE;
   }

   /* A6:verify R_B (far end ephemeral key) is on curve*/
   err = pka_ecc_point_validate(state, curve, far_ephkey);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_ecc_point_validate", err);
      return err;
   }

   /* A5:compute t_A = d_A + x1' * r_A */
   /* t = x1' * r_A */
   /* convert x1 to x1' */
   memset(tmp, 0, size/2);
   memcpy(tmp+size/2, my_ephkey_pt.x+size/2, size/2);
   tmp[size/2] |= 0x80;
   err = pka_core_ecc_modmult(state, tmp, my_ephkey, n, np, nr, t, size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_modmult", err);
      return err;
   }
   /* t = d_A + x1' * r_A */
   err =  pka_core_ecc_modadd(state, my_key, size, t, n, t, size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_ecc_modadd", err);
      return err;
   }

   u_point.x   = u_point_x;
   u_point.y   = u_point_y;

   /* A7: compute point U = [h*t_A] (P_B + [x2']R_B) */
   /* P_B far_key point */
   /* R_B far_ephkey point */
   /* x2 is x coord of R_B */

   /* t = h * t mod p */
   /* h = 1 so no computation needed */
   /* convert x2 to x2' */
   memset(tmp, 0, size/2);
   memcpy(tmp+size/2, far_ephkey.x+size/2, size/2);
   tmp[size/2] |= 0x80;
   /* tmp = t * [x2'] */
   err = pka_core_ecc_modmult(state, t, tmp, n, np, nr, tmp, size);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("pka_core_modmult", err);
      return err;
   }

   /* Try Shamir's Trick first */
   /* u_point = t * P_B + t * [x2']R_B */
   err = pka_core_pmult2add(state, far_key, t, far_ephkey, tmp, m, a, mp, r_sqr, size, u_point);
   if (err != PKA_OK)
   {
      if (err == PKA_RC_REASON_INVALID_POINT)
      {
         /* Try again without using Shamir */
         struct ecc_point tmp_point;
         uint8_t    tmp_point_x[size];
         uint8_t    tmp_point_y[size];

         tmp_point.x = tmp_point_x;
         tmp_point.y = tmp_point_y;

         /* u_point = t * [x2']R_B */
         err = pka_core_ecc_pmult(state, far_ephkey, tmp, b, m, a, mp, r_sqr, size, u_point);
         if (err != PKA_OK)
         {
            PKA_REPORT_ERR("pka_core_ecc_pmult", err);
            return err;
         }

         /* tmp_point = t * P_B */
         err = pka_core_ecc_pmult(state, far_key, t, b, m, a, mp, r_sqr, size, tmp_point);
         if (err != PKA_OK)
         {
            PKA_REPORT_ERR("pka_core_ecc_pmult", err);
            return err;
         }

         /* u_point = [t * P_B] + [t * [x2']R_B */
         err = pka_core_ecc_point_add(state, tmp_point, u_point, m, a, mp, r_sqr, size, u_point);
         if (err != PKA_OK)
         {
            PKA_REPORT_ERR("pka_core_ecc_point_add", err);
            return err;
         }
      }
      else
      {
         PKA_REPORT_ERR("Shamir fail", err);
         return err;
      }
   }

   /* A8: compute KDF */
   /* initialize kdf with (x2||y2||Z_A||Z_B) */
   zbufp = zbuf;
   memcpy(zbufp, u_point.x, size);
   zbufp += size;
   memcpy(zbufp, u_point.y, size);
   zbufp += size;
   memcpy(zbufp, (role == SM2_INITIATOR) ? my_userhash : far_userhash, MAX_SM2_HASH_BLOCKSIZE);
   zbufp += MAX_SM2_HASH_BLOCKSIZE;
   memcpy(zbufp, (role == SM2_INITIATOR) ? far_userhash : my_userhash, MAX_SM2_HASH_BLOCKSIZE);

   pka_sm2_kdf_init(&kdfstate, zbuf, SM2_ZBUF_SIZE);

   /* fill outlen with kdf blocks */
   while (outlen > 0) {
      kdfsize = chunksize = SM3_SIZE;

      err = pka_sm2_kdf_getblock(&kdfstate, kdfblock, &kdfsize);
      if (err != PKA_OK)
      {
         /* bail OUT if KDF gives all 0 for any block */
         PKA_REPORT_ERR("pka_sm2_kdf_getblock", err);
         return err;
      }

      if (kdfsize < chunksize) {
         chunksize = kdfsize;
      }

      memcpy(out, kdfblock, chunksize);
      outlen -= chunksize;
      out += chunksize;
   }

   return err;
}
