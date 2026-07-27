# SPDX-License-Identifier: Apache-2.0
# sw/smc-vp-tests/smc-beu-error-test/Makefile
#
# Phase E2/E3 of the BEU platform integration plan
# (smc/peripherals/beu/doc/04_BEU_Platform_Integration_Test_Plan.md): unlike
# smc-beu-test (E1, register smoke only), this test needs the platform's
# test-only CCI-driven error-injection hook (Phase D1) enabled, so it points
# VP_FLAGS at its own .ini instead of the shared default.
#
# NOTE: TARGET uses underscores (not "smc-beu-error-test") for the same
# reason as smc-beu-test (see that Makefile) -- avoids colliding with stale
# directory-name .gitignore patterns.
SRCS   = ../common/start.S main.c ../common/printf.c
OBJS   = $(SRCS:.c=.o)
OBJS  := $(OBJS:.S=.o)
TARGET = smc_beu_error_test
LDFLAGS += -T ../common/link.ld

VP_FLAGS ?= smc_beu_error_test.ini

include ../Makefile.common
