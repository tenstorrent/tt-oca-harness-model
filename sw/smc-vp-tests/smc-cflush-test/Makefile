# SPDX-License-Identifier: Apache-2.0
# sw/smc-vp-tests/smc-cflush-test/Makefile
#
# TARGET uses underscores so the binary never collides with the directory name.
SRCS   = ../common/start.S main.c ../common/printf.c
OBJS   = $(SRCS:.c=.o)
OBJS  := $(OBJS:.S=.o)
TARGET = smc_cflush_test
LDFLAGS += -T ../common/link.ld

include ../Makefile.common
