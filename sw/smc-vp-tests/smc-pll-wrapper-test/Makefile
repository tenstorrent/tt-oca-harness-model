# SPDX-License-Identifier: Apache-2.0
# sw/smc-vp-tests/smc-pll-wrapper-test/Makefile
#
# NOTE: TARGET uses underscores (not "smc-pll-wrapper-test") so the built
# binary name never collides with a directory-name .gitignore pattern.
SRCS   = ../common/start.S main.c ../common/printf.c
OBJS   = $(SRCS:.c=.o)
OBJS  := $(OBJS:.S=.o)
TARGET = smc_pll_wrapper_test
LDFLAGS += -T ../common/link.ld

# Uses the shared default platform ini (no per-test CCI overrides needed).
include ../Makefile.common
