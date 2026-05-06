/***************************************************************************
 * PKA Sample App entry points - C linkage for C++ harness integration
 *
 * These functions are implemented in the PKA sample app .c files.
 * Use extern "C" when including from C++ to avoid name mangling.
 ***************************************************************************/

#ifndef PKA_SAMPLE_APPS_H
#define PKA_SAMPLE_APPS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Each returns 0 on success, non-zero on failure */

int rsa_sign_verify_nist_main(void);
int rsa_encrypt_decrypt_main(void);
int ecc_sign_verify_asn1_main(void);
int ecc_sign_verify_bp_p384r1_main(void);
int ecc_encrypt_decrypt_bp_p256r1_main(void);
int ecc_key_agree_nist384_main(void);
int eddsa_sign_verify_main(void);
int montg_key_agree_main(void);
int sm2_sign_verify_256_main(void);
int sm2_encrypt_decrypt_256_main(void);
int sm2_key_agree_256_main(void);

#ifdef __cplusplus
}
#endif

#endif /* PKA_SAMPLE_APPS_H */
