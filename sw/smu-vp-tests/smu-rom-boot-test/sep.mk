# SPDX-License-Identifier: Apache-2.0
# sw/smu-vp-tests/smu-rom-boot-test/sep.mk
#
# SEP (RV32 VeeR) half of the SMU ROM-only boot test.  Reuses the sep-vp-tests
# startup/printf/linker infrastructure; built via `make -f sep.mk` from the
# top-level Makefile in this directory (never included directly).
SRCS   = ../../sep-vp-tests/common/start.S sep_main.c ../../sep-vp-tests/common/printf.c
OBJS   = $(SRCS:.c=.o)
OBJS  := $(OBJS:.S=.o)
TARGET = smu_rom_boot_sep
LDFLAGS += -T ../../sep-vp-tests/common/link.ld

include ../../sep-vp-tests/Makefile.common
