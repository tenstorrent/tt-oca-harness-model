PKA Driver Package

------------------------------------------------------------------------
This Synopsys software and associated documentation (hereinafter the
"Software") is an unsupported proprietary work of Synopsys, Inc. unless
otherwise expressly agreed to in writing between Synopsys and you. The
Software IS NOT an item of Licensed Software or a Licensed Product under
any End User Software License Agreement or Agreement for Licensed Products
with Synopsys or any supplement thereto. Synopsys is a registered trademark
of Synopsys, Inc. Other names included in the SOFTWARE may be the
trademarks of their respective owners.

The contents of this file are dual-licensed; you may select either version
2 of the GNU General Public License ("GPL") or the BSD-3-Clause license
("BSD-3-Clause"). The GPL is included in the COPYING file accompanying the
SOFTWARE. The BSD License is copied below.

BSD-3-Clause License:
Copyright (c) 2011-2023 Synopsys, Inc. and/or its affiliates.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions, and the following disclaimer, without
   modification.

2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

3. The names of the above-listed copyright holders may not be used to
   endorse or promote products derived from this software without specific
   prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
------------------------------------------------------------------------

This package provides reference code demonstrating how to perform
various cryptographic operations such as sign, verify, encrypt, decrypt
using public key algorithms such as RSA and ECC using the DWC_PKA
product. Reference documentation for the driver and APIs is located in
the file synopsys/pka/doc/PKAAPI.chm.
It also provides the plugin code to the Crypto Software Library (CSL).
The reference documentation for the plugin code is located in the file
synopsys/pka/doc/DWC_cryptography_software_library_mw_pka_plugin_user_guide.pdf.


This package was tested on a 32-bit ARM platform with Linux kernel
version 4.9.99.

1.00-LCA01
- Initial release of PKA Driver.

1.00GA-RC1
- Plugin to the Crypto Software Library (CSL) added.

1.00GA
- General Available release.

2.20b
- Same as 1.00GA with versioning only changed to match the hardware version.

2.21a
- Enhancement: Added support for the frp256v1 curve
- Bug fix: pka_ecc_sig_ver is missing a check on r and s of the signature
- Bug fix: Brainpool curves do not work when using CSL plugin
- Bug fix: pka_ecc_sig_gen and pka_ecc_sig_ver are missing a reduction step for r
- Bug fix: pka_core_go entry argument width is too small
- Bug fix: hash_k shared between pka_eddsa_sign and pka_eddsa_verify
- Bug fix: memory leaks in csl_pka_modexp_crt and csl_pka_modexp ( CSL plugin )
- Bug fix: remove incorrect sections in DWC_cryptography_software_library_mw_pka_plugin_user_guide.pdf
- Bug fix: fixed sample code function pka_sm2_make_user_hash() arguments in sm2_key_agree_256.c
- Bug fix: fixed sample code function pka_sm2_sign_hash() arguments in sm2_sign_verify_256.c
- Bug fix: fixed SHA512 HMAC code usage
- Bug fix: fixed errors in RSA and ECC sample applications
- Bug fix: fixed ASN1 DER encoding/decoding for processing PKA signatures with possible leading zeros
- Enhancement: added HW acceleration for RSA key generation ( CSL plugin mode ELP_MATH_PLUGIN_V2 )
- Enhancement: moved macro definitions to internal header ( src/include/data_macros.h )
