# SPDX-License-Identifier: Apache-2.0
# sw/smc-vp-tests/smc-telemetry-test/Makefile
#
# TARGET uses underscores so the binary name never collides with a
# directory-name .gitignore pattern.
SRCS   = ../common/start.S main.c ../common/printf.c
OBJS   = $(SRCS:.c=.o)
OBJS  := $(OBJS:.S=.o)
TARGET = smc_telemetry_test
LDFLAGS += -T ../common/link.ld

# Opt into elaboration-time ATB inject so the firmware can see a message.
VP_FLAGS ?= smc_telemetry_test.ini

include ../Makefile.common
