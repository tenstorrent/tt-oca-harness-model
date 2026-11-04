# SPDX-License-Identifier: Apache-2.0
# sw/smu-vp-tests/smu-lifecycle-test/smc.mk
#
# SMC (RV64 Rocket) half of the SMU lifecycle test.  Reuses the smc-vp-tests
# startup/printf/linker infrastructure; built via `make -f smc.mk` from the
# top-level Makefile in this directory (never included directly).
SRCS   = ../../smc-vp-tests/common/start.S smc_main.c ../../smc-vp-tests/common/printf.c
OBJS   = $(SRCS:.c=.o)
OBJS  := $(OBJS:.S=.o)
TARGET = smu_lifecycle_smc
LDFLAGS += -T ../../smc-vp-tests/common/link.ld

include ../../smc-vp-tests/Makefile.common
