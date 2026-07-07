# SPI Mux + Flash-Loader Test Makefile

SRCS = ../common/start.S main.c ../common/printf.c
OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)
TARGET = main
LDFLAGS += -T ../common/link.ld

include ../Makefile.common

# --- Flash fixture staging -------------------------------------------------
# sep-vp chdir's to the directory of the .ini it is given, so the flash model resolves
# data/flash_memory.bin relative to the CONFIG directory (not this test dir). Stage there.
CFG_DIR     := $(dir $(VP_FLAGS))
STAGED_IMG  := $(CFG_DIR)data/flash_memory.bin
SIM_TIMEOUT ?= 20

.PHONY: fixture stage unstage sim-unstaged sim-staged distclean

flash_fixture.bin: gen_flash_fixture.py
	python3 gen_flash_fixture.py $@

fixture: flash_fixture.bin

stage: flash_fixture.bin
	mkdir -p $(CFG_DIR)data && cp flash_fixture.bin $(STAGED_IMG)

unstage:
	rm -f $(STAGED_IMG)
	-rmdir $(CFG_DIR)data 2>/dev/null || true

# Convenience: build+run the two verification modes. Each forces a clean rebuild so the
# FLASH_STAGED expectation in main.c matches the staging state, and wraps the sep-vp run in a
# timeout (the VP does not self-terminate).
sim-unstaged:
	$(MAKE) clean
	$(MAKE) unstage
	$(MAKE) $(TARGET)
	timeout -k 5 $(SIM_TIMEOUT) $(VP) $(VP_FLAGS) $(TARGET) || true

sim-staged:
	$(MAKE) clean
	$(MAKE) stage
	$(MAKE) EXTRA_CFLAGS=-DFLASH_STAGED $(TARGET)
	timeout -k 5 $(SIM_TIMEOUT) $(VP) $(VP_FLAGS) $(TARGET) || true
	$(MAKE) unstage

distclean: clean
	$(MAKE) unstage
	rm -f flash_fixture.bin
