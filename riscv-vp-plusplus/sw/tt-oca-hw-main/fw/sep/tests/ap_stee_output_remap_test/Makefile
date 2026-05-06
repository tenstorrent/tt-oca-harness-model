# AP / STEE Test Makefile
#
# Copyright 2025 Tenstorrent Inc.

TEST = ap_stee_output_remap_test
export TEST

# Include paths for register headers
EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common

# Explicitly add picolibc library path (GCC multilib selects rv32ima but picolibc has rv32imac)
PICOLIBC_LIB = $(PICOLIBC_DIR)/picolibc/$(GCC_PREFIX)/lib/rv32imac/ilp32
EXT_LD_FLAGS = -L$(PICOLIBC_LIB) --oslib=dummyhost

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

all: ap_stee_output_remap_test.elf