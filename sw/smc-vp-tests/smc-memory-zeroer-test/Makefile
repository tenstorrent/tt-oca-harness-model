# SPDX-License-Identifier: Apache-2.0
# sw/smc-vp-tests/smc-memory-zeroer-test/Makefile
#
# NOTE: TARGET uses underscores (not "smc-memory-zeroer-test") so the built
# binary name never collides with a directory-name .gitignore pattern (see
# the stale entries in ../.gitignore for smc-scratch-test / smc-uart-test /
# etc., which accidentally shadow whole test directories of the same name).
SRCS   = ../common/start.S main.c ../common/printf.c
OBJS   = $(SRCS:.c=.o)
OBJS  := $(OBJS:.S=.o)
TARGET = smc_memory_zeroer_test
LDFLAGS += -T ../common/link.ld

# Needs chunk_size=8 (see smc_memory_zeroer_test.ini header comment) instead
# of the shared default ini.
VP_FLAGS ?= smc_memory_zeroer_test.ini

include ../Makefile.common
