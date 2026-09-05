# OTBN P256 ECDSA Verification Test Makefile

# Source files
SRCS = ../common/start.S otbn_p256_verify_test.c ../common/printf.c p256_ecdsa_otbn.c
OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)
TARGET = otbn-dv-p256-verify-test
LDFLAGS += -T ../common/link.ld

# Include common build infrastructure
include ../Makefile.common

# The shared sep-vp config selects otbn_loop so the insn-count firmware test
# passes. This test needs the OpenSSL P256 verify algorithm instead — the VP
# does not interpret IMEM, it dispatches on algorithm_type.
# Sibling ini lives next to accellera_config.ini so SEP_VP_INI_DIR / relative
# paths (spiPreload, configFile, targets) keep resolving.
TEST_INI := $(dir $(BASE_INI))accellera_config_p256_ecdsa.ini
VP_FLAGS := $(TEST_INI)
