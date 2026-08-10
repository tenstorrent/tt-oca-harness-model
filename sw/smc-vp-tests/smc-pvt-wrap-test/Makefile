# SPDX-License-Identifier: Apache-2.0
# sw/smc-vp-tests/smc-pvt-wrap-test/Makefile
#
# NOTE: TARGET uses underscores (not "smc-pvt-wrap-test") so the built binary
# name never collides with a directory-name .gitignore pattern.
SRCS = ../common/start.S main.c ../common/printf.c
OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)
TARGET = smc_pvt_wrap_test
LDFLAGS += -T ../common/link.ld

include ../Makefile.common
