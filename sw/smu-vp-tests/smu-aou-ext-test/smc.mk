# SPDX-License-Identifier: Apache-2.0
SRCS   = ../../smc-vp-tests/common/start.S smc_main.c ../../smc-vp-tests/common/printf.c
OBJS   = $(SRCS:.c=.o)
OBJS  := $(OBJS:.S=.o)
TARGET = smu_aou_ext_smc
LDFLAGS += -T ../../smc-vp-tests/common/link.ld

include ../../smc-vp-tests/Makefile.common
