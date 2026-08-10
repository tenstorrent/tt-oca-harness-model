# SPI Mux + Flash-Loader Test Makefile

SRCS = ../common/start.S main.c ../common/printf.c
OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)
TARGET = main
LDFLAGS += -T ../common/link.ld

include ../Makefile.common

# --- Flash fixture staging -------------------------------------------------
# Two verification modes, selected with STAGED:
#   make sim            no image in flash, main.c expects erased 0xFF
#   make sim STAGED=1   fixture staged, main.c expects its manifest magic
# Both need the bootcode preload out of the way (see NO_SPIPRELOAD_INI in
# ../Makefile.common) -- it fills the flash with boot data, which is neither erased nor the
# fixture. The fixture is staged in the config directory because sep-vp chdir's there, so
# the flash model resolves data/flash_memory.bin relative to it, not to this test dir.
VP_FLAGS   := $(NO_SPIPRELOAD_INI)
CFG_DIR    := $(dir $(NO_SPIPRELOAD_INI))
STAGED_IMG := $(CFG_DIR)data/flash_memory.bin

STAGED ?= 0
ifeq ($(STAGED),1)
EXTRA_CFLAGS += -DFLASH_STAGED
endif

# main.c compiles to different expectations per mode, so switching modes has to force a
# rebuild -- otherwise stale objects from the other mode produce a bogus result. Rewrite
# the stamp only when the mode actually changes, and hang the objects off it.
$(shell [ "`cat .staged-mode 2>/dev/null`" = "$(STAGED)" ] || echo $(STAGED) > .staged-mode)
$(OBJS): .staged-mode

.PHONY: fixture stage unstage unstage-image clean-mode distclean

flash_fixture.bin: gen_flash_fixture.py
	python3 gen_flash_fixture.py $@

fixture: flash_fixture.bin

$(STAGED_IMG): flash_fixture.bin
	mkdir -p $(dir $@) && cp $< $@

stage: $(STAGED_IMG)

unstage-image:
	rm -f $(STAGED_IMG)
	-rmdir $(CFG_DIR)data 2>/dev/null || true

unstage: unstage-image
	rm -f $(NO_SPIPRELOAD_INI)

# Give `sim` (from Makefile.common) the state its mode expects, so plain `make sim` works
# like every other test here.
ifeq ($(STAGED),1)
sim: $(NO_SPIPRELOAD_INI) $(STAGED_IMG)
else
sim: $(NO_SPIPRELOAD_INI) unstage-image
endif

clean-mode:
	rm -f .staged-mode

clean: unstage clean-mode

distclean: clean
	rm -f flash_fixture.bin
