# SPDX-License-Identifier: Apache-2.0
SRCS   = ../../smc-vp-tests/common/start.S smc_main.c ../../smc-vp-tests/common/printf.c
OBJS   = $(SRCS:.c=.o)
OBJS  := $(OBJS:.S=.o)
TARGET = smu_aou_delay_smc
LDFLAGS += -T ../../smc-vp-tests/common/link.ld

# BRIDGE_DELAY_NS / MTIME_TICK_NS are passed down by the Makefile so the
# firmware checks the same values the generated smc.ini configures.  When
# unset (direct `make -f smc.mk`) smc_main.c falls back to its own defaults.
ifdef BRIDGE_DELAY_NS
EXTRA_CFLAGS += -DBRIDGE_DELAY_NS=$(BRIDGE_DELAY_NS)u
endif
ifdef MTIME_TICK_NS
EXTRA_CFLAGS += -DMTIME_TICK_NS=$(MTIME_TICK_NS)u
endif

include ../../smc-vp-tests/Makefile.common

# Rebuild the test object when the Makefile's stamp says the values changed.
# (wildcard: no stamp when smc.mk is run standalone.)
smc_main.o: $(wildcard .params.stamp)
