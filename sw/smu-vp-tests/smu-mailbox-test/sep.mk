# SPDX-License-Identifier: Apache-2.0
SRCS   = ../../sep-vp-tests/common/start.S sep_main.c ../../sep-vp-tests/common/printf.c
OBJS   = $(SRCS:.c=.o)
OBJS  := $(OBJS:.S=.o)
TARGET = smu_mailbox_sep
LDFLAGS += -T ../../sep-vp-tests/common/link.ld

include ../../sep-vp-tests/Makefile.common
