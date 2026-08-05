# SPDX-License-Identifier: Apache-2.0
SRCS   = ../common/start.S main.c ../common/printf.c
OBJS   = $(SRCS:.c=.o)
OBJS  := $(OBJS:.S=.o)
TARGET = smc_aou_test
LDFLAGS += -T ../common/link.ld
SIM_TIME_MS ?= 50

include ../Makefile.common
