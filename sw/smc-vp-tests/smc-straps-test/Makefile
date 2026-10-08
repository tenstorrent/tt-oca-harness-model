# SPDX-License-Identifier: Apache-2.0
# sw/smc-vp-tests/smc-straps-test/Makefile
#
# TARGET uses underscores so the binary never collides with the directory name.
SRCS   = ../common/start.S main.c ../common/printf.c
OBJS   = $(SRCS:.c=.o)
OBJS  := $(OBJS:.S=.o)
TARGET = smc_straps_test
LDFLAGS += -T ../common/link.ld

# Needs a non-zero dut.captured_straps (see the .ini header).
VP_FLAGS ?= smc_straps_test.ini

include ../Makefile.common
