# WDT Sanity Test Makefile
#
# This test verifies the AON timer watchdog bark interrupt triggers NMI.
#
# Copyright 2025 Tenstorrent Inc.

TEST = wdt_sanity_test
export TEST

# Include paths for register headers
EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/dv/sep/tests/common

# Use custom linker script with NMI handler section
LINKER_SCRIPT = exec_from_tcms_wdt.ld

# Explicitly add picolibc library path (GCC multilib selects rv32ima but picolibc has rv32imac)
PICOLIBC_LIB = $(PICOLIBC_DIR)/picolibc/$(GCC_PREFIX)/lib/rv32imac/ilp32
EXT_LD_FLAGS = -L$(PICOLIBC_LIB) --oslib=dummyhost

# Use our custom crt0_wdt.s instead of wildcard picking up local .s files
ASM_SRCS = crt0_wdt.s

# Override COMMON_ASM_SRCS to empty so common/crt0.s is not used
# Note: common.mk will override this, so we also override COMMON_OBJS
COMMON_ASM_SRCS =

include $(OCH_ROOT)/dv/sep/tests/common/common.mk

# Override COMMON_OBJS to exclude common/crt0.o (only use init_stdout.o)
# Since common.mk expands prerequisites at parse time, we need to override COMMON_OBJS
COMMON_OBJS = $(COMMON_C_SRCS:.c=.o)

# Include the .nmi_handler section in ITCM (common.mk only includes .text)
# This MUST be after the include to override common.mk's definition
ITCM_ELF_SECTIONS = --only-section=.text --only-section=.nmi_handler

# Add crt0_wdt.o as an additional prerequisite to the elf rule
# This ensures it gets built (the pattern rule %.o: %.s will handle building it)
$(TEST).elf: crt0_wdt.o

all: wdt_sanity_test.elf
