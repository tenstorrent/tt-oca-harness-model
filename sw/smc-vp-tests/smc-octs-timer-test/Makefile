# SPDX-License-Identifier: Apache-2.0
# sw/smc-vp-tests/smc-octs-timer-test/Makefile
#
# TARGET uses underscores (not "smc-octs-timer-test") so the built binary name
# never collides with a directory-name .gitignore pattern -- see the note in
# smc-memory-zeroer-test/Makefile.
SRCS   = ../common/start.S main.c ../common/printf.c
OBJS   = $(SRCS:.c=.o)
OBJS  := $(OBJS:.S=.o)
TARGET = smc_octs_timer_test
LDFLAGS += -T ../common/link.ld

include ../Makefile.common
