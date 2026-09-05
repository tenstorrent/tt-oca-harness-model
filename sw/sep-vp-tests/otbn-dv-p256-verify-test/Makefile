# OTBN P256 ECDSA Verification Test Makefile

# Source files
SRCS = ../common/start.S otbn_p256_verify_test.c ../common/printf.c p256_ecdsa_otbn.c
OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)
TARGET = otbn-dv-p256-verify-test
LDFLAGS += -T ../common/link.ld

# Include common build infrastructure
include ../Makefile.common

# Platform default is otbn_loop (needed by otbn-dv-test). This test loads a
# real P-256 program and OpenTitan vectors, so it must select p256_ecdsa.
# Sibling ini lives next to accellera_config.ini so SEP_VP_INI_DIR / relative
# paths (spiPreload, configFile, targets) keep resolving.
VP_FLAGS := $(dir $(BASE_INI))accellera_config_p256_ecdsa.ini
