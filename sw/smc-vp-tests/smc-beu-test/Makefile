# SPDX-License-Identifier: Apache-2.0
# sw/smc-vp-tests/smc-beu-test/Makefile
#
# NOTE: TARGET uses underscores (not "smc-beu-test") so the built binary
# name never collides with a directory-name .gitignore pattern (see the
# stale entries in ../.gitignore for smc-scratch-test / smc-uart-test /
# etc., which accidentally shadow whole test directories of the same name).
SRCS   = ../common/start.S main.c ../common/printf.c
OBJS   = $(SRCS:.c=.o)
OBJS  := $(OBJS:.S=.o)
TARGET = smc_beu_test
LDFLAGS += -T ../common/link.ld

include ../Makefile.common
