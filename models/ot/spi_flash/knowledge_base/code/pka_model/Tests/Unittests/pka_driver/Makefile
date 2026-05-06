
PKA_MAIN = $(CURDIR)
SRC = $(PKA_MAIN)/src
HDRS = -I$(PKA_MAIN)/include \
       -I$(SRC)/include
SAMPLES = $(PKA_MAIN)/sample_app

WARNINGS = -Wall -Werror

CFLAGS += -DSTD_ERR=1

OBJ_RSA_ENCRYPT_DECRYPT  = $(SAMPLES)/rsa_encrypt_decrypt.o
OBJ_RSA_SIGN_VERIFY_NIST = $(SAMPLES)/rsa_sign_verify_nist.o

OBJ = $(OBJ_RSA_ENCRYPT_DECRYPT) $(OBJ_RSA_SIGN_VERIFY_NIST)

OBJ_ECC_SIGN_VERIFY_BP_P384R1 = $(SAMPLES)/ecc_sign_verify_bp_p384r1.o
OBJ                      += $(OBJ_ECC_SIGN_VERIFY_BP_P384R1)
OBJ_EDDSA_SIGN_VERIFY    = $(SAMPLES)/eddsa_sign_verify.o
OBJ                      += $(OBJ_EDDSA_SIGN_VERIFY)
OBJ_ECC_KEY_AGREE_NIST_384   = $(SAMPLES)/ecc_key_agree_nist384.o
OBJ_MONTG_KEY_AGREE          = $(SAMPLES)/montg_key_agree.o
OBJ_ENCRYPT_DECRYPT_BP_P256R1 = $(SAMPLES)/ecc_encrypt_decrypt_bp_p256r1.o
OBJ_SM2_ENCRYPT_DECRYPT_256   = $(SAMPLES)/sm2_encrypt_decrypt_256.o
OBJ_ECC_SIGN_VERIFY_DER       = $(SAMPLES)/ecc_sign_verify_asn1.o
OBJ_SM2_SIGN_VERIFY_256       = $(SAMPLES)/sm2_sign_verify_256.o
OBJ_SM2_KEY_AGREE_256         = $(SAMPLES)/sm2_key_agree_256.o
OBJ                      += $(OBJ_ECC_KEY_AGREE_NIST_384) \
                            $(OBJ_MONTG_KEY_AGREE) \
                            $(OBJ_ENCRYPT_DECRYPT_BP_P256R1) \
                            $(OBJ_ECC_SIGN_VERIFY_DER) \
                            $(OBJ_SM2_SIGN_VERIFY_256) \
                            $(OBJ_SM2_ENCRYPT_DECRYPT_256) \
                            $(OBJ_SM2_KEY_AGREE_256)

OUTPUT_RSA_ENCRYPT_DECRYPT  = $(SAMPLES)/rsa_encrypt_decrypt
OUTPUT_RSA_SIGN_VERIFY_NIST = $(SAMPLES)/rsa_sign_verify_nist
OUT = $(OUTPUT_RSA_ENCRYPT_DECRYPT) $(OUTPUT_RSA_SIGN_VERIFY_NIST)

OUTPUT_ECC_SIGN_VERIFY_BP_P384R1 = $(SAMPLES)/ecc_sign_verify_bp_p384r1
OUT                         += $(OUTPUT_ECC_SIGN_VERIFY_BP_P384R1)
OUTPUT_EDDSA_SIGN_VERIFY    = $(SAMPLES)/eddsa_sign_verify
OUT                         += $(OUTPUT_EDDSA_SIGN_VERIFY)
OUTPUT_ECC_KEY_AGREE_NIST_384  = $(SAMPLES)/ecc_key_agree_nist384
OUTPUT_MONTG_KEY_AGREE         = $(SAMPLES)/montg_key_agree
OUTPUT_ENCRYPT_DECRYPT_BP_P256R1 = $(SAMPLES)/ecc_encrypt_decrypt_bp_p256r1
OUTPUT_ECC_SIGN_VERIFY_DER       = $(SAMPLES)/ecc_sign_verify_asn1
OUTPUT_SM2_SIGN_VERIFY_256       = $(SAMPLES)/sm2_sign_verify_256
OUTPUT_SM2_ENCRYPT_DECRYPT_256   = $(SAMPLES)/sm2_encrypt_decrypt_256
OUTPUT_SM2_KEY_AGREE_256         = $(SAMPLES)/sm2_key_agree_256
OUT                         += $(OUTPUT_ECC_KEY_AGREE_NIST_384) \
                               $(OUTPUT_MONTG_KEY_AGREE) \
                               $(OUTPUT_ENCRYPT_DECRYPT_BP_P256R1) \
                               $(OUTPUT_ECC_SIGN_VERIFY_DER) \
                               $(OUTPUT_SM2_SIGN_VERIFY_256) \
                               $(OUTPUT_SM2_ENCRYPT_DECRYPT_256) \
                               $(OUTPUT_SM2_KEY_AGREE_256)

OBJECTS  = $(SRC)/sha512.o $(SRC)/sm3.o $(SRC)/pka_setup.o $(SRC)/pka_driv.o $(SRC)/pka_core.o \
           $(SRC)/pka_rsa.o $(SRC)/pka_core_rsa.o \
           $(SRC)/pka_ecc.o $(SRC)/pka_core_ecc.o \
           $(SRC)/pka_montg_ed.o $(SRC)/pka_core_montg_ed.o $(SRC)/asn1_der.o

OBJECTS_MIN = $(SRC)/pka_setup.o $(SRC)/pka_driv.o $(SRC)/pka_core.o \
           $(SRC)/pka_core_rsa.o \
           $(SRC)/pka_core_ecc.o

default: samples

LIB = libpka.a

size: lib
	size $(LIB)

lib: $(LIB)

$(LIB): $(OBJECTS)
	$(AR) rs $@ $^

%.o: %.c
	$(CC) $(WARNINGS) $(HDRS) $(CFLAGS) -c $< -o $@


$(OUTPUT_SHA512):
	$(CC) $(HDRS) $(LDFLAGS) $(OBJECTS) $(OBJ_SHA512) -o $(OUTPUT_SHA512)
$(OUTPUT_RSA_SIGN_VERIFY_NIST):
	$(CC) $(HDRS)  $(LDFLAGS) $(OBJECTS) $(OBJ_RSA_SIGN_VERIFY_NIST) -o $(OUTPUT_RSA_SIGN_VERIFY_NIST)
$(OUTPUT_RSA_ENCRYPT_DECRYPT):
	$(CC) $(HDRS)  $(LDFLAGS) $(OBJECTS) $(OBJ_RSA_ENCRYPT_DECRYPT) -o $(OUTPUT_RSA_ENCRYPT_DECRYPT)
$(OUTPUT_ECC_SIGN_VERIFY_BP_P384R1):
	$(CC) $(HDRS)  $(LDFLAGS) $(OBJECTS) $(OBJ_ECC_SIGN_VERIFY_BP_P384R1) -o $(OUTPUT_ECC_SIGN_VERIFY_BP_P384R1)
$(OUTPUT_ECC_KEY_AGREE_NIST_384):
	$(CC) $(HDRS)  $(LDFLAGS) $(OBJECTS) $(OBJ_ECC_KEY_AGREE_NIST_384) -o $(OUTPUT_ECC_KEY_AGREE_NIST_384)
$(OUTPUT_MONTG_KEY_AGREE):
	$(CC) $(HDRS)  $(LDFLAGS) $(OBJECTS) $(OBJ_MONTG_KEY_AGREE) -o $(OUTPUT_MONTG_KEY_AGREE)
$(OUTPUT_EDDSA_SIGN_VERIFY):
	$(CC) $(HDRS)  $(LDFLAGS) $(OBJECTS) $(OBJ_EDDSA_SIGN_VERIFY) -o $(OUTPUT_EDDSA_SIGN_VERIFY)
$(OUTPUT_ENCRYPT_DECRYPT_BP_P256R1):
	$(CC) $(HDRS)  $(LDFLAGS) $(OBJECTS) $(OBJ_ENCRYPT_DECRYPT_BP_P256R1) -o $(OUTPUT_ENCRYPT_DECRYPT_BP_P256R1)
$(OUTPUT_ECC_SIGN_VERIFY_DER):
	$(CC) $(HDRS) $(LDFLAGS) $(OBJECTS) $(OBJ_ECC_SIGN_VERIFY_DER) -o $(OUTPUT_ECC_SIGN_VERIFY_DER)
$(OUTPUT_SM2_SIGN_VERIFY_256):
	$(CC) $(HDRS) $(LDFLAGS) $(OBJECTS) $(OBJ_SM2_SIGN_VERIFY_256) -o $(OUTPUT_SM2_SIGN_VERIFY_256)
$(OUTPUT_SM2_ENCRYPT_DECRYPT_256):
	$(CC) $(HDRS)  $(LDFLAGS) $(OBJECTS) $(OBJ_SM2_ENCRYPT_DECRYPT_256) -o $(OUTPUT_SM2_ENCRYPT_DECRYPT_256)
$(OUTPUT_SM2_KEY_AGREE_256):
	$(CC) $(HDRS)  $(LDFLAGS) $(OBJECTS) $(OBJ_SM2_KEY_AGREE_256) -o $(OUTPUT_SM2_KEY_AGREE_256)

samples:  $(OBJECTS) $(OBJ) $(OUT)

clean:
	-rm -f $(OBJECTS) $(OBJ) $(OUT) $(LIB)
