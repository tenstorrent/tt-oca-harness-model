# SPDX-License-Identifier: Apache-2.0
SRCS   = ../common/start.S main.c ../common/printf.c
OBJS   = $(SRCS:.c=.o)
OBJS  := $(OBJS:.S=.o)
TARGET = smc_i3c_loopback_test
LDFLAGS += -T ../common/link.ld

include ../Makefile.common
