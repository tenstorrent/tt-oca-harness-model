# SPDX-License-Identifier: Apache-2.0
# sw/smc-vp-tests/smc-map-coherence-test/Makefile
#
# Cumulative Phase-1 address-map coherence check (D4=H1).
SRCS   = ../common/start.S main.c ../common/printf.c
OBJS   = $(SRCS:.c=.o)
OBJS  := $(OBJS:.S=.o)
TARGET = smc_map_coherence_test
LDFLAGS += -T ../common/link.ld
SIM_TIME_MS ?= 100

include ../Makefile.common
