# SPI-controller -> secure-DMA streaming integration test Makefile

SRCS = ../common/start.S main.c ../common/printf.c
OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)
TARGET = main
LDFLAGS += -T ../common/link.ld

include ../Makefile.common

# --- Dedicated VP config + flash fixture -----------------------------------
# This test verifies DMA-drained bytes against an image in the SPI flash, so it needs the
# bootcode preload out of the way (see NO_SPIPRELOAD_INI in ../Makefile.common). The
# fixture has to be staged in the config directory too: sep-vp chdir's there, so the flash
# model resolves data/flash_memory.bin relative to it rather than to this test dir.
VP_FLAGS    := $(NO_SPIPRELOAD_INI)
CFG_DIR     := $(dir $(NO_SPIPRELOAD_INI))
STAGED_IMG  := $(CFG_DIR)data/flash_memory.bin

.PHONY: fixture stage unstage distclean

flash_fixture.bin: gen_flash_fixture.py
	python3 gen_flash_fixture.py $@

fixture: flash_fixture.bin

$(STAGED_IMG): flash_fixture.bin
	mkdir -p $(dir $@) && cp $< $@

stage: $(STAGED_IMG)

unstage:
	rm -f $(STAGED_IMG) $(NO_SPIPRELOAD_INI)
	-rmdir $(CFG_DIR)data 2>/dev/null || true

# Give `sim` (from Makefile.common) the config and fixture as prerequisites so plain
# `make sim` works like every other test here. Without the fixture every read compares
# against an empty flash and all four transfers fail.
sim: $(NO_SPIPRELOAD_INI) $(STAGED_IMG)

clean: unstage

distclean: clean
	rm -f flash_fixture.bin
