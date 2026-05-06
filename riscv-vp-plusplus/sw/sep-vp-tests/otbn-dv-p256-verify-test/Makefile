# OTBN P256 ECDSA Verification Test Makefile

# Source files
SRCS = start.S otbn_p256_verify_test.c printf.c p256_ecdsa_otbn.c
OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)
TARGET = otbn-dv-p256-verify-test

# Include common build infrastructure
include ../Makefile.common
