# SPDX-License-Identifier: Apache-2.0
# sw/smc-vp-tests/smc-avsbus-test/Makefile
#
# NOTE: TARGET uses underscores (not "smc-avsbus-test") so the built binary
# name never collides with a directory-name .gitignore pattern (see the
# stale entries in ../.gitignore for smc-scratch-test / smc-uart-test /
# etc., which accidentally shadow whole test directories of the same name).
SRCS   = ../common/start.S main.c ../common/printf.c
OBJS   = $(SRCS:.c=.o)
OBJS  := $(OBJS:.S=.o)
TARGET = smc_avsbus_test
LDFLAGS += -T ../common/link.ld
# Allow wall-clock sim time for the AVSBus xfer_delay_ns (~50 ns default)
# plus register traffic / printf.
SIM_TIME_MS ?= 100

include ../Makefile.common
