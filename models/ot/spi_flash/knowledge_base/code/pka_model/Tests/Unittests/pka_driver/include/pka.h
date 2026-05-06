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
#ifndef PKA_H_
#define PKA_H_

#include "pka_defines.h"

#define software_version "DWC_PKA_Driver_2.21a"

/**
 * \defgroup Entropy Random Data
 * @{
 * \ingroup B_Entropy
 * \details
 * This function generates random data required by different operations in the driver. This function maps
 * to the #pka_driv_random function in the driver layer.
 *
 * \param [in/out]  data      Pointer to entropy data
 * \param [in]      size      Size in bytes of entropy data
 *
 * \return
*     - #PKA_OK         Successful
 *    - #errorType      Hardware or Software failures as defined by the errorType
 *
 */
int pka_random(uint8_t *data, uint32_t size);
/**
 * @}
 */
/**
 * \defgroup RSAPrecomp RSA precomputation
 * @{
 * \ingroup B_RSAOperations
 * \details
 * This function performs the precomputation required for some RSA operations to populate r_sqr and mp
 * in the #pka_rsa_mod modulus structure. If you use the same key repetitively for many RSA operations this function
 * need only be called once to generate the precomputed values.
 *
 * \param [in]  state          PKA instance pointer
 * \param [in]  modlen         Modulus size
 * \param [in/out]  mod        Modulus structure
 *
 * \return
 *    - #PKA_OK         Successful
 *    - #errorType      Hardware or Software failures as defined by the errorType
 *
 */
int pka_rsa_precomp(struct pka_state *state, struct pka_rsa_mod *mod, uint16_t modlen);
/**
 * @}
 */
/**
 * \defgroup RSASign RSA Sign
 * @{
 * \ingroup B_RSAOperations
 * \details
 * This function generates a signature with a private key using RSA. The input message hash type
 * is selectable (#hashType), and the private key format is selectable as per PKCS #1. The message signature
 * size is defined by the size of the key (i.e. a 2048 bit key requires a 256 byte buffer)
 * <br> 
 * The two private key formats are RSA_key which is encoded as (n, d), or RSA_CRT_KEY encoded asn (p, q, dP, dQ, qInv),
 * as defined in PKCS #1:RSA Cryptography Specifications Version 2.2, Section 3.3.
 *
 * \param [in]  state      PKA instance pointer
 * \param [in]  key        Private key structure for signing
 * \param [in]  format     Format of the data in the key structure. 1: RSA_key format, 2: RSA_CRT_key
 * \param [in]  hash       Pointer to the hash
 * \param [in]  htype      Message hash type (#hashType)
 * \param [out] signature  Pointer to buffer where signature will be stored.
 *
 * \return
 *    - #PKA_OK         Successful
 *    - #errorType      Hardware or Software failures as defined by the errorType
 *
 */
int pka_rsa_sign(struct pka_state *state, void *key, enum pka_rsa_key_format format, uint8_t *hash, enum hashType htype, uint8_t *signature);
/**
 * @}
 */

/**
 * \defgroup RSAVerify RSA Verify
 * @{
 * \ingroup B_RSAOperations
 * \details
 * This function verifies a signature using RSA as specified in PKCS #1. The input message hash type
 * is selectable and is compared against the signature which was created with the private key corresponding
 * to the public key used by this function.
 *
 *
 * \param [in] state       PKA instance pointer
 * \param [in] key         Pre-computed public key structure. See #pka_rsa_precomp.
 * \param [in] hash        Message hash
 * \param [in] htype       Message hash type (#hashType)
 * \param [in] signature   The signature to verify (length is determined by the key size)
 *
 * \return
 *    - #PKA_OK         Successful
 *    - #errorType      Hardware or Software failures as defined by the errorType
 *
 */
int pka_rsa_verify(struct pka_state *state, struct pka_rsa_key *key, uint8_t *hash, enum hashType htype, uint8_t *signature);
/**
 * @}
 */
/**
 * \defgroup RSAEncrypt RSA Encrypt
 * @{
 * \ingroup B_RSAOperations
 * \details
 * This function encrypts a message with a private key using RSA with PKCS1-V1_5 (EME) encoding
 * to generate the encrypted output. If the message length per the size of the key used
 * (the mod_size field in the #pka_rsa_key struct) is too long then a #PKA_INVRNGE error is returned.
 * The padding size (PS) is determined by the size of the key
 * used (the mod_size field in the #pka_rsa_key struct). If PS too small or the random number length is
 * too small then a #PKA_INVPARAM is returned.
 *
 * \param [in]  state     PKA instance pointer
 * \param [in]  key       Pre-computed public key structure. See #pka_rsa_precomp.
 * \param [in]  m         Message to encrypt
 * \param [in]  mlen      Input message length in bytes
 * \param [in]  rng       Random number
 * \param [in]  rnglen    Random number length in bytes, must be greater than: key size in bytes - (mlen + 3)
 * \param [out] ct        Encrypted message. The length of the message is determined by the size of the key
 *
 * \return
 *    - #PKA_OK         Successful
 *    - #errorType      Hardware or Software failures as defined by the errorType
 *
 */
int pka_rsa_encrypt(struct pka_state *state, struct pka_rsa_key *key , uint8_t *m, uint32_t mlen, uint8_t *rng, uint16_t rnglen, uint8_t *ct);
/**
 * @}
 */
/**
 * \defgroup RSADecrypt RSA Decrypt
 * @{
 * \ingroup B_RSAOperations
 * \details
 * This function decrypts a message with the public key using RSA.
 * <br> 
 * The two private key formats are RSA_key which is encoded as (n, d), or RSA_CRT_KEY encoded asn (p, q, dP, dQ, qInv),
 * as defined in PKCS #1:RSA Cryptography Specifications Version 2.2, Section 3.3.
 *
 * \param [in]  state    PKA instance pointer
 * \param [in]  key      Pre-computed public key structure. See #pka_rsa_precomp.
 * \param [in]  format   Format for the private key. 1: RSA_key 2: RSA_CRT_key
 * \param [in]  ct       Encrypted data. The size is based on the key used (mod_size)
 * \param [out] msg      Pointer to where to store the decrypted message
 * \param [out] msglen   Message length in bytes
 *
 * \return
 *    - #PKA_OK         Successful
 *    - #errorType      Hardware or Software failures as defined by the errorType
 *
 */
int pka_rsa_decrypt(struct pka_state *state, void *key, enum pka_rsa_key_format format, uint8_t *ct, uint8_t *msg, uint32_t msglen);
/**
 * @}
 */
/**
* \defgroup ECDSASign ECDSA Signing Operation
* @{
* \ingroup C_ECCOperations
*
* \details
* This function generates the r,s signature pair of a message hash by performing the ECDSA sign operation as
* specified in ANS X9.62. All big integer parameter lengths are based on the key size defined by #eccCurveByteSize.
*
*/
/**
* \param [in]  state        PKA instance pointer
* \param [in]  curve        Curve selection #eccCurve
* \param [in]  key          Private key big integer
* \param [in]  b            Random blinding big integer
* \param [in]  k            Random number big integer
* \param [in]  hash         Message hash to be signed
* \param [in]  hlen         Message hash length in bytes
* \param [out] r            Signature component r big integer
* \param [out] s            Signature component s big integer
*
*
* \return
*    - #PKA_OK         Successful
*    - #errorType      Hardware or Software failures as defined by the errorType
*
*/
int pka_ecc_sign_hash(struct pka_state *state, enum eccCurve curve, uint8_t *key, uint8_t *b, uint8_t *k, uint8_t *hash, uint32_t hlen, uint8_t *r, uint8_t *s);
/**
 * @}
 */
/**
* \defgroup SM2Sign Signing Operation
* @{
* \ingroup F_SM2Operations
*
* \details
* This function generates the r,s signature pair of a message hash by performing an SM2 sign operation as specified in
* GM/T 0003-2012, Part 2. All big integer parameter lengths are based on the key size defined by #PKA_CURVE_SM2_SCA256_BYTE.
*/
/**
* \param [in]  state        PKA instance pointer
* \param [in]  curve        Curve selection #eccCurve (must be #PKA_SW_CURVE_SM2_SCA256)
* \param [in]  key          Private key big integer
* \param [in]  b            Random blinding big integer
* \param [in]  k            Random big integer
* \param [in]  hash         Message hash to be signed
* \param [in]  hlen         Message hash length in bytes
* \param [out] r            Signature component r big integer
* \param [out] s            Signature component s big integer
*
*
* \return
*    - #PKA_OK         Successful
*    - #errorType      Hardware or Software failures as defined by the errorType
*
*/
int pka_sm2_sign_hash(struct pka_state *state, enum eccCurve curve, uint8_t *key, uint8_t *b, uint8_t *k, uint8_t *hash, uint32_t hlen, uint8_t *r, uint8_t *s);
/**
* @}
*/
/**
* \defgroup ECDSASignASN1 ECDSA Signing Operation (ASN.1)
* @{
 * \ingroup C_ECCOperations
* \details
* This function generates a ASN.1 DER format message hash signature of size sig_len.
* It performs an ECDSA sign operation for the signature pair r,s as specified in specified in ANS X9.62
* and then provides the resulting signature in ASN.1 DER format. All big integer parameter lengths are
* based on the key size defined by #eccCurveByteSize.
*/
/**
* \param [in]  state        PKA instance pointer
* \param [in]  curve        Curve selection #eccCurve
* \param [in]  key          Private key big integer
* \param [in]  b            Public key #ecc_point (Qx, Qy) big integer pair
* \param [in]  k            Random number big integer
* \param [in]  hash         Hash message to be signed
* \param [in]  hlen         Hash message length in bytes
* \param [out] signature    Signature in ASN.1 DER format
* \param [out] sig_len      Signature length in bytes
*
* \return
*    - #PKA_OK         Successful
*    - #errorType      Hardware or Software failures as defined by the errorType
*
*/
int pka_ecc_sign_hash_der(struct pka_state *state, enum eccCurve curve, uint8_t *key, uint8_t *b, uint8_t *k, uint8_t *hash, uint32_t hlen, uint8_t * signature, uint32_t *sig_len);
/**
* @}
*/
/**
* \defgroup SM2SignASN1 Signing Operation (ASN.1)
* @{
 * \ingroup F_SM2Operations
* \details
* This function generates a message hash signature in ASN.1 DER format.
* It performs an SM2 sign operation for the signature pair r,s as specified in GM/T 0003-2012, Part 2
* and then provides the resulting signature in ASN.1 DER format.  All big integer parameter lengths
* are based on the key size #PKA_CURVE_SM2_SCA256_BYTE.
*/
/**
* \param [in]  state        PKA instance pointer
* \param [in]  curve        Curve selection #eccCurve (must be #PKA_SW_CURVE_SM2_SCA256)
* \param [in]  key          Private key big integer
* \param [in]  b            Random blinding big integer
* \param [in]  k            Random big integer
* \param [in]  hash         Hash message to be signed
* \param [in]  hlen         Hash message length in bytes
* \param [out] signature    Signature in ASN.1 DER format
* \param [out] sig_len      Signature length in bytes
*
* \return
*    - #PKA_OK         Successful
*    - #errorType      Hardware or Software failures as defined by the errorType
*
*/
int pka_sm2_sign_hash_der(struct pka_state *state, enum eccCurve curve, uint8_t *key, uint8_t *b, uint8_t *k, uint8_t *hash, uint32_t hlen, uint8_t * signature, uint32_t *sig_len);
/**
* @}
*/
/**
* \defgroup ECDSAverify ECDSA Verification Operation
* \ingroup C_ECCOperations
* @{
*
* \details
* This function verifies the ECDSA signature pair r,s generated from #pka_ecc_sign_hash as specified in ANS X9.62.
* It requires the public key #ecc_point generated by #pka_ecc_key_generate with the private key used
* in #pka_ecc_sign_hash along with the message hash to verify against. All big integer parameter lengths are
* based on the key size defined by #eccCurveByteSize.
*/
/**
* \param [in] state        PKA instance pointer
* \param [in] curve        Curve selection #eccCurve
* \param [in] keypub       Public key #ecc_point (Qx, Qy) big integer pair
* \param [in] hash         Hash message to verify against the signature
* \param [in] hlen         Hash message length in bytes
* \param [in] r            Signature component r big integer
* \param [in] s            Signature component s big integer
*
* \return
*    - #PKA_OK         Successful
*    - #errorType      Hardware or Software failures as defined by the errorType
*
*/
int pka_ecc_verify_hash(struct pka_state *state, enum eccCurve curve, struct ecc_point keypub, uint8_t *hash, uint32_t hlen, uint8_t *r, uint8_t *s);
/**
* @}
*/
/**
* \defgroup SM2verify Verification Operation
* \ingroup F_SM2Operations
* @{
*
* \details
* This function verifies the SM2 signature pair r,s generated from #pka_sm2_sign_hash as specified in GM/T 0003-2012, Part 2.
* It requires the public key #ecc_point generated by #pka_sm2_key_generate with the private key used
* in #pka_sm2_sign_hash along with a message hash to verify against. All big integer parameter lengths are
* based on the key size #PKA_CURVE_SM2_SCA256_BYTE.
*/
/**
* \param [in] state        PKA instance pointer
* \param [in] curve        Curve selection #eccCurve (must be #PKA_SW_CURVE_SM2_SCA256)
* \param [in] keypub       Public key #ecc_point (Qx, Qy) big integer pair
* \param [in] hash         Hash message to verify against the signature
* \param [in] hlen         Hash message length in bytes
* \param [in] r            Signature component r big integer
* \param [in] s            Signature component s big integer
*
* \return
*    - #PKA_OK         Successful
*    - #errorType      Hardware or Software failures as defined by the errorType
*
*/
int pka_sm2_verify_hash(struct pka_state *state, enum eccCurve curve, struct ecc_point keypub, uint8_t *hash, uint32_t hlen, uint8_t *r, uint8_t *s);
/**
* @}
*/
/**
* \defgroup ECDSAverifyASN1 ECDSA Verification Operation (ASN.1)
* \ingroup C_ECCOperations
* @{
*
* \details
* This function performs a ECDSA verify operation taking the message hash signature in ASN.1 DER format
* from #pka_ecc_sign_hash_der.  It decodes the signature to a signature pair r, s and then uses
* #pka_ecc_verify_hash to verify the signature.  All big integer parameter lengths are based on the
* key size defined by #eccCurveByteSize.
*/
/**
* \param [in] state        PKA instance pointer
* \param [in] curve        Curve selection #eccCurve
* \param [in] keypub       Public key #ecc_point (Qx, Qy) big integer pair
* \param [in] hash         Hash message to verify
* \param [in] hlen         Hash message length in bytes
* \param [in] signature    Signature in ASN.1 DER format
* \param [in] sig_len      Signature length in bytes
*
* \return
*    - #PKA_OK         Successful
*    - #errorType      Hardware or Software failures as defined by the errorType
*
*/
int pka_ecc_verify_hash_der(struct pka_state *state, enum eccCurve curve, struct ecc_point keypub, uint8_t *hash, uint32_t hlen, uint8_t *signature, uint32_t sig_len);
/**
* @}
*/
/**
* \defgroup SM2verifyASN1 Verification Operation (ASN.1)
* \ingroup F_SM2Operations
* @{
*
* \details
* This function performs a SM2 verify operation taking the message hash signature in ASN.1 DER format
* from #pka_sm2_sign_hash_der.  It decodes the signature to a signature pair r, s and then uses
* #pka_sm2_verify_hash to verify the signature.  All big integer parameter lengths are based on the
* key size #PKA_CURVE_SM2_SCA256_BYTE.
*/
/**
* \param [in] state        PKA instance pointer
* \param [in] curve        Curve selection #eccCurve (must be #PKA_SW_CURVE_SM2_SCA256)
* \param [in] keypub       Public key #ecc_point (Qx, Qy) big integer pair
* \param [in] hash         Hash message to verify
* \param [in] hlen         Hash message length in bytes
* \param [in] signature    Signature in ASN.1 DER format
* \param [in] sig_len      Signature length in bytes
*
* \return
*    - #PKA_OK         Successful
*    - #errorType      Hardware or Software failures as defined by the errorType
*
*/
int pka_sm2_verify_hash_der(struct pka_state *state, enum eccCurve curve, struct ecc_point keypub, uint8_t *hash, uint32_t hlen, uint8_t *signature, uint32_t sig_len);
/**
* @}
*/
/**
* \defgroup ECCKeyPair ECDSA Key Pairing
*
* @{
* \ingroup C_ECCOperations
* \details
* This function generates the ECC public key #ecc_point from the private key.
* A point multiply using the curve selection base point and the private key provides
* the output public key #ecc_point point.  All big integer parameter lengths are
* based on the key size defined by #eccCurveByteSize.
*/
/**
* \param [in]  state    PKA instance pointer
* \param [in]  curve    Curve selection #eccCurve
* \param [in]  dprime   Private key big integer
* \param [in]  b        Random blinding big integer
* \param [out] keypub   Public key #ecc_point (Qx, Qy) big integer pair
*
*
* \return
*    - #PKA_OK         Successful
*    - #errorType      Hardware or Software failures as defined by the errorType
*
*/
int pka_ecc_key_generate(struct pka_state *state, enum eccCurve curve, uint8_t *dprime, uint8_t *b, struct ecc_point keypub);
/**
* @}
*/
/**
* \defgroup SM2KeyPair Key Pairing
*
* @{
* \ingroup F_SM2Operations
* \details
* This function generates the SM2 public key #ecc_point from the SM2 private key.
* The keytype is used to select the private key range checking. SM2_SIGNING_KEY (1 to order-2)
* or SM2_KEY_AGREEMENT_KEY (1 to order-1). A point multiply using the curve selection base
* point and the private key provides the public key #ecc_point.  All big integer parameter
* lengths are based on the key size #PKA_CURVE_SM2_SCA256_BYTE.
*/
/**
* \param [in]  state    PKA instance pointer
* \param [in]  curve    Curve selection #eccCurve (must be #PKA_SW_CURVE_SM2_SCA256)
* \param [in]  dprime   Private key big integer
* \param [in]  b        Random blinding big integer
* \param [in]  keytype  SM2_SIGNING_KEY or SM2_KEY_AGREEMENT_KEY #sm2_key_type
* \param [out] keypub   Public key #ecc_point (Qx, Qy) big integer pair
*
* \return
*    - #PKA_OK         Successful
*    - #errorType      Hardware or Software failures as defined by the errorType
*
*/
int pka_sm2_key_generate(struct pka_state *state, enum eccCurve curve, uint8_t *dprime,
                         uint8_t *b, enum sm2_key_type keytype, struct ecc_point keypub);
/**
* @}
*/
/**
* \defgroup SM2UserHash User Hash
* @{
* \ingroup F_SM2Operations
* \details
* This function generates a user hash from the SM2 public #ecc_point key
* according to GM/T 0003-2012 part 3 section 5.5.  It is used as part of
* SM2 key agreement and SM2 sign verify.
* It requires input for the user id string uid_str, user id length in bytes uidlen,
* public SM2 key pubkey generated by #pka_sm2_key_generate using the private key.
* It provides the output out for the user hash and uses the input hash size outlen in bytes.
* All big integer parameter lengths are based on the key size #PKA_CURVE_SM2_SCA256_BYTE.
*/
/**
* \param [in]  uid_str   String containing UID
* \param [in]  uidlen    Length of UID string in bytes
* \param [in]  pubkey    Public key #ecc_point generated from private SM2 key
* \param [out] out       Destination for the user hash
* \param [in]  outlen    Size of the out hash buffer in bytes
*
* \return
*    - #PKA_OK         Successful
*    - #errorType      Hardware or Software failures as defined by the errorType
*/
int pka_sm2_make_user_hash(const char *uid_str, uint16_t uidlen,
                           struct ecc_point pubkey, uint8_t *out, uint32_t *outlen);
/**
* @}
*/
/**
* \defgroup SM2MsgHash Message Hash
* @{
* \ingroup F_SM2Operations
* \details
* This function generates the message hash from the SM2 public #ecc_point key
* according to GM/T 0003-2012 part 1 section 6.1, A1/A2.  It is used as part of
* the SM2 sign verify.
* It requires input for the user hash from the function pka_sm2_make_user_hash out,
* message, message length in bytes. It provides the output out for the message hash
* and uses the input hash size outlen in bytes.
*/
/**
* \param [in]  user_hash     User hash buffer
* \param [in]  user_hash_len Size of hash buffer in bytes
* \param [in]  msg           Message
* \param [in]  msg_len       Size of message in bytes
* \param [out] out           Destination for the message hash
* \param [in]  outlen        Size of the out hash buffer in bytes
*
* \return
*    - #PKA_OK         Successful
*    - #errorType      Hardware or Software failures as defined by the errorType
*/
int pka_sm2_make_msg_hash(const uint8_t *user_hash, uint32_t user_hash_len,
                          const uint8_t *msg, uint32_t msg_len,
                          uint8_t *out, uint32_t *outlen);
/**
* @}
*/
/**
* \defgroup ECIESEnc Encrypt
* @{
* \ingroup  C_ECCOperations
* \details
* This function performs an ECC encryption of a message as specified in ANS X9.63-2011, Section 5.8.1.
* It uses the KDF (Key Derivation Function) algorithm specified Section 5.6.3 with SHA-512 to generate the key
* used to encrypt the message. The final output is generated with a HMAC algorithm as per the specification.
* All big integer parameter lengths are based on the key size defined by #eccCurveByteSize.
*/
/**
*
* \param [in] state        PKA instance pointer
* \param [in] curve        curve selection #eccCurve
* \param [in] key          Public key #ecc_point (Qx, Qy) big integer pair
* \param [in] dprime       Private key big integer
* \param [in] b            Random blinding big integer
* \param [in] msg          Message to encrypt
* \param [in] mlen         Input message length in bytes
* \param [out] encdata     Encrypted data. Size is: ((2 * big_integer_size) + 1 + mlen + #TAG_SIZE)
*
*
* \return
*    - #PKA_OK         Encrypt is Successful
*    - #errorType      Hardware or Software failures as defined by the errorType
*
*/
int pka_ecc_encrypt(struct pka_state *state, enum eccCurve curve, struct ecc_point key, uint8_t *dprime, uint8_t *b, uint8_t *msg, uint32_t mlen, uint8_t *encdata);
/**
* @}
*/
/**
* \defgroup SM2Enc Encrypt
* @{
* \ingroup  F_SM2Operations
* \details
* This function performs a SM2 encryption of a message as specified in GM/T 0003-2012, Part 4.
* All big integer parameter lengths are based on the key size #PKA_CURVE_SM2_SCA256_BYTE.
*/
/**
*
* \param [in] state        PKA instance pointer
* \param [in] curve        Curve selection #eccCurve (must be #PKA_SW_CURVE_SM2_SCA256)
* \param [in] key          Public key #ecc_point (Qx, Qy) big integer pair
* \param [in] dprime       Random big integer
* \param [in] b            Random blinding big integer
* \param [in] msg          Message to encrypt
* \param [in] mlen         Input message length in bytes
* \param [out] encdata     Encrypted message. Size is: (1 + 2*#PKA_CURVE_SM2_SCA256_BYTE + #SM3_SIZE + mlen)
*
*
* \return
*    - #PKA_OK         Encrypt is Successful
*    - #errorType      Hardware or Software failures as defined by the errorType
*
*/
int pka_sm2_encrypt(struct pka_state *state, enum eccCurve curve, struct ecc_point key, uint8_t *dprime, uint8_t *b, uint8_t *msg, uint32_t mlen, uint8_t *encdata);
/**
* @}
*/
/**
* \defgroup ECIESDec Decrypt
* @{
* \ingroup  C_ECCOperations
* \details
* This function performs an ECC decryption of an encrypted message from #pka_ecc_encrypt as specified in ANS X9.63-201, Section 5.8.2.
* The output decrypted message out is of byte size message length or encdatalen - (2*cp_size + 1 + taglen).
* All big integer parameter lengths are based on the key size defined by #eccCurveByteSize.
*/
/**
*
* \param [in] state        PKA instance pointer
* \param [in] curve        Curve selection #eccCurve
* \param [in] dprime       Random big integer
* \param [in] b            Random blinding big integer
* \param [in] encdata      Encrypted message
* \param [in] encdatalen   Encrypted message length in bytes
* \param [out] outdata     Decrypted message
*
*
* \return
*    - #PKA_OK         Decrypt is Successful
*    - #errorType      Hardware or Software failures as defined by the errorType
*
*/
int pka_ecc_decrypt(struct pka_state *state, enum eccCurve curve, uint8_t *dprime, uint8_t *b,  uint8_t *encdata, uint32_t encdatalen, uint8_t *outdata);
/**
* @}
*/
/**
* \defgroup SM2Dec Decrypt
* @{
* \ingroup  F_SM2Operations
* \details
* This function performs a SM2 decryption of an encoded message from #pka_sm2_encrypt as specified in GM/T 0003-2012, Part 4.
* All big integer parameter lengths are of size #PKA_CURVE_SM2_SCA256_BYTE.
*/
/**
*
* \param [in] state        PKA instance pointer.
* \param [in] curve        Curve selection #eccCurve (must be #PKA_SW_CURVE_SM2_SCA256)
* \param [in] dprime       Random big integer
* \param [in] b            Random blinding big integer
* \param [in] encdata      Encrypted message
* \param [in] encdatalen   Encrypted message length in bytes
* \param [out] outdata     Decrypted message length: (encdatalen - (1 + 2*#PKA_CURVE_SM2_SCA256_BYTE + #SM3_SIZE))
*
*
* \return
*    - #PKA_OK         Decrypt is Successful
*    - #errorType      Hardware or Software failures as defined by the errorType
*
*/
int pka_sm2_decrypt(struct pka_state *state, enum eccCurve curve, uint8_t *dprime, uint8_t *b, uint8_t *encdata, uint32_t encdatalen, uint8_t *outdata);
/**
* @}
*/
/**
* \defgroup ECDHKeyAgree ECDSA Key Agreement
* @{
* \ingroup  C_ECCOperations
* \details
* This function generates a shared secret point (Px, Py) #ecc_point using ECC.
* All big integer parameter lengths are
* based on the key size defined by #eccCurveByteSize.
*/
/**
*
* \param [in]  state    PKA instance pointer
* \param [in]  curve    Curve selection #eccCurve
* \param [in]  keypriv  Private key big integer
* \param [in]  keypub   Public key (Qx, Qy) #ecc_point big integer pair
* \param [in]  b        Random blinding big integer
* \param [out] secret   Shared secret (Px, Py) #ecc_point big integer pair
*
*
* \return
*    - #PKA_OK         Successful
*    - #errorType      Hardware or Software failures as defined by the errorType
*
*/
int pka_ecc_key_agreement(struct pka_state *state, enum eccCurve curve, struct ecc_point keypub, uint8_t *keypriv, uint8_t *b, struct ecc_point secret);
/**
* @}
*/
/**
* \defgroup SM2KeyAgree Key Agreement
* @{
* \ingroup  F_SM2Operations
* \details
* This function generates a shared secret key using SM2 as specified in GM/T 0003-2012, Part 5.
* To create the shared key this function requires the far and near end hash, public and
* ephemeral keys, a blinding value and the agreement role.
*
*/
/**
*
* \param [in]  state        PKA instance pointer.
* \param [in]  curve        Curve selection #eccCurve (must be #PKA_SW_CURVE_SM2_SCA256)
* \param [in]  far_key      Far end public key #ecc_point big integer pair
* \param [in]  far_ephkey   Far end ephemeral key #ecc_point big integer pair
* \param [in]  far_userhash Pointer to far end user hash
* \param [in]  my_key       Pointer to near end private key big integer
* \param [in]  my_ephkey    Pointer to near end ephemeral key big integer
* \param [in]  my_ephkey_pt Near end ephemeral public key #ecc_point big integer pair
* \param [in]  my_userhash  Pointer to near end user hash
* \param [in]  b            Pointer to random blinding big integer
* \param [in]  role         Near end role in exchange, initiator or responder #sm2_key_agree_role
* \param [out] out          Pointer to output buffer
* \param [in]  outlen       Pointer to output buffer size which must be #PKA_CURVE_SM2_SCA256_BYTE
*
* \return
*    - #PKA_OK         Successful
*    - error_Type      Hardware or Software failures as defined in pka_defines.h error_Type.
*
*/
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
                          uint32_t                outlen);
/**
* @}
*/
/**
* \defgroup ECCKeyValidate ECC Key Validate
* \ingroup C_ECCOperations
* @{
* \details
* This function validates an ECC key (point) to ensure it is on the curve, as described by ANS X9.63-2011, Section 5.2.2.1.
* All big integer parameter lengths are based on the key size defined by #eccCurveByteSize.
*/
/**
*
* \param [in]  state    PKA instance pointer
* \param [in]  curve    Curve selection #eccCurve
* \param [in]  key      Key #ecc_point (Qx, Qy) big integer pair
*
*
* \return
*    - #PKA_OK         Successful
*    - #errorType      Hardware or Software failures as defined by the errorType
*/
int pka_ecc_point_validate(struct pka_state *state, enum eccCurve curve, struct ecc_point key);
/**
* @}
*/
/**
* \defgroup EdDSAKeyPair Edwards curve Key Pairing
* \ingroup D_Curve25519
* @{
*
* \details
* This function generates a public key from the private key as specified in: RFC 8032, 5.1.5.
* All big integer parameter lengths are of size #PKA_SW_CURVE_ED25519.
*/
/**
* \param [in]  state    PKA instance pointer
* \param [in]  curve    Curve selection #eccCurve (must use #PKA_SW_CURVE_ED25519)
* \param [in]  keypriv  Private key big integer
* \param [in]  b        Random blinding big integer
* \param [out] keypub   Public key big integer
*
*
* \return
*    - #PKA_OK         Successful
*    - #errorType      Hardware or Software failures as defined by the errorType
*
*/
int pka_eddsa_key_gen(struct pka_state *state, enum eccCurve curve,
                      uint8_t *keypriv, uint8_t *b, uint8_t *keypub);
/**
* @}
*/
/**
* \defgroup EdDSASign Edwards curve Signing
* \ingroup D_Curve25519
* @{
* \details
* This function performs an EdDSA sign message operation and provides the signature pair r,s
* as described in RFC 8032 5.1.6.  All big integer parameter lengths are based on the key size
* #PKA_SW_CURVE_ED25519.
*
*/
/**
* \param [in]  state    PKA instance pointer
* \param [in]  curve    Curve selection #eccCurve (must use #PKA_SW_CURVE_ED25519)
* \param [in]  keypriv  Private key big integer
* \param [in]  keypub   Public key big integer
* \param [in]  b        Random blinding big integer
* \param [in]  msg      Message
* \param [in]  msg_len  Message length in bytes
* \param [out] sig_r    signature r big integer
* \param [out] sig_s    signature s big integer
*
*
* \return
*    - #PKA_OK         Successful
*    - #errorType      Hardware or Software failures as defined by the errorType
*
*/
int pka_eddsa_sign(struct pka_state *state, enum eccCurve curve,
                    uint8_t *keypriv, uint8_t *keypub,
                    uint8_t *b, uint8_t *msg, uint32_t msg_len,
                    uint8_t *sig_r, uint8_t *sig_s);
/**
* @}
*/
/**
* \defgroup EdDSAVerify Edwards curve Verification
* \ingroup D_Curve25519
* @{
*
* \details
* This function verifies the signature pair r,s using the public key keypub all from #pka_eddsa_sign as
* described in RFC 8032 5.1.7.  All big integer parameter lengths are based on the key size #PKA_SW_CURVE_ED25519.
*
*/
/**
* \param [in] state        PKA instance pointer.
* \param [in] curve        Curve selection #eccCurve (must use #PKA_SW_CURVE_ED25519)
* \param [in] sig_r        Encrypted point R big integer
* \param [in] sig_s        Signature s big integer to verify as defined in RFC 8032 (5.1.6.6)
* \param [in] msg          Message
* \param [in] msg_len      Message length in bytes
* \param [in] keypub       Public key big integer
*
*
* \return
*    - #PKA_OK         Verify is Successful
*    - #errorType      Hardware or Software failures as defined by the errorType
*
*/
int pka_eddsa_verify(struct pka_state *state, enum eccCurve curve, uint8_t *sig_r, uint8_t *sig_s,
            uint8_t *msg, uint32_t msg_len, uint8_t *keypub);
/**
* @}
*/
/**
* \defgroup x25519 Montgomery curve 25519
* \ingroup D_Curve25519
* @{
*
* \details
* This function performs a Montgomery point multiplication function on Curve25519 and 
* generates a result key.  All big integer parameter lengths are based on the
* key size #PKA_CURVE_CURVE25519_BYTE.
*/
/**
*
* \param [in]  state   PKA instance pointer
* \param [in]  curve   Curve selection #eccCurve (must use #PKA_SW_CURVE_CURVE25519)
* \param [in]  k       Integer private key big integer
* \param [in]  u       Encoded public key big integer
* \param [in]  b       Random blinding big integer
* \param [out] c       Result key big integer
*
*
* \return
*    - #PKA_OK         Successful
*    - #errorType      Hardware or Software failures as defined by the errorType
*
*/
int pka_x25519(struct pka_state *state, enum eccCurve curve,
               uint8_t *k, uint8_t *u, uint8_t *b, uint8_t *c);

/**
* @}
*/
/**
 * \defgroup PKALIBSETUP initialize software library
 * \ingroup A_PKASetup
 * @{
 *
 *
 * \details
 * This function initializes the software. The software self-configures by querying the appropriate registers in the PKA
 * to determine its build configuration and supports only key sizes based on the instantiated hardware.
 *
 * The loading of the firmware is optional depending on the style of instruction memory created for the PKA. For ROM only versions
 * loading of the firmware memory is not required, otherwise you must provide a pointer to the firmware to load.
 *
 * \param [in]  state          PKA instance pointer.
 * \param [in]  pka_regbase    Physical base address of the PKA registers.
 * \param [in]  fw             Pointer to the clp300_ram_fw.hex (converted to ram_fw.h) file created when running the PKA
 *                             corekit (located in the /sim/casm folder).
 *                             A NULL pointer means firmware will not be loaded (i.e. ROM only build).
 * \param [in]  fwlen          Length of the firmware to load.
 *
 * \return
 *    - #PKA_OK         Successful
 *    - #errorType      Hardware or Software failures as defined by the errorType
 *
 */
int pka_lib_setup(struct pka_state *state,  uint32_t pka_regbase, uint8_t *fw, uint16_t fwlen);
/**
* @}
*/
/**
* \ingroup A_PKAClose
* @{
*
*
* \details
*  Call to exit to clean up.
*
* \return
*    - #PKA_OK      Successful
*    - #errorType   Hardware or Software failures as defined by the errorType
*/
int pka_lib_close(struct pka_state *state);
/**
* @}
*/
#endif

