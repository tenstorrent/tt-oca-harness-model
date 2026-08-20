# SPDX-License-Identifier: Apache-2.0
# sw/smc-vp-tests/smc-dma-test/Makefile
#
# NOTE: TARGET uses underscores (not "smc-dma-test") so the built binary name
# never collides with a directory-name .gitignore pattern (see ../.gitignore).
SRCS   = ../common/start.S main.c ../common/printf.c
OBJS   = $(SRCS:.c=.o)
OBJS  := $(OBJS:.S=.o)
TARGET = smc_dma_test
LDFLAGS += -T ../common/link.ld

include ../Makefile.common
