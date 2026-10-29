# SPDX-License-Identifier: Apache-2.0
# sw/smc-vp-tests/smc-zb-isa-test/Makefile
#
# TARGET uses underscores so the binary never collides with the directory name.
SRCS   = ../common/start.S main.c ../common/printf.c
OBJS   = $(SRCS:.c=.o)
OBJS  := $(OBJS:.S=.o)
TARGET = smc_zb_isa_test
LDFLAGS += -T ../common/link.ld

# The SMC cores implement Zba, Zbb and Zbs; build for them, as the SMC Boot ROM does.
ARCH_CANDIDATES = rv64imac_zicsr_zifencei_zba_zbb_zbs rv64imac_zba_zbb_zbs

include ../Makefile.common
