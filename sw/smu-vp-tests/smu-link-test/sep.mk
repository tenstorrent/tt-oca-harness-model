# SPDX-License-Identifier: Apache-2.0
# sw/smu-vp-tests/smu-link-test/sep.mk
#
# SEP (RV32 VeeR) half of the SMU link test.  Reuses the sep-vp-tests
# startup/printf/linker infrastructure; built via `make -f sep.mk` from the
# top-level Makefile in this directory (never included directly).
SRCS   = ../../sep-vp-tests/common/start.S sep_main.c ../../sep-vp-tests/common/printf.c
OBJS   = $(SRCS:.c=.o)
OBJS  := $(OBJS:.S=.o)
TARGET = smu_link_sep
LDFLAGS += -T ../../sep-vp-tests/common/link.ld

include ../../sep-vp-tests/Makefile.common
