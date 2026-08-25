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
# Both need the bootcode preload out of the way -- it fills the flash with boot data, which
# is neither erased nor the fixture. The two modes differ only in which config they run:
# the staged one names the image, the erased one names nothing. That is what makes the
# erased mode trustworthy -- the flash model loads only what a config asks for, so no
# leftover file can turn erased flash into fixture bytes. The filename is test-specific so
# it cannot collide with another test's fixture. Paths are relative to the config, and
# sep-vp chdir's there.
STAGED_INI  := $(dir $(BASE_INI))accellera_config_spi_mux_test.ini
FIXTURE_REL := data/sep_spi_mux_test_flash.bin
STAGED_IMG  := $(dir $(BASE_INI))$(FIXTURE_REL)

$(STAGED_INI): $(BASE_INI)
	sed 's|^och_sep_ss1\.spiPreload|#&|' $< > $@
	echo 'och_sep_ss1.spiBackdoorFile : "$(FIXTURE_REL)"' >> $@

STAGED ?= 0
ifeq ($(STAGED),1)
EXTRA_CFLAGS += -DFLASH_STAGED
VP_FLAGS   := $(STAGED_INI)
else
VP_FLAGS   := $(NO_SPIPRELOAD_INI)
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
	-rmdir $(dir $(STAGED_IMG)) 2>/dev/null || true

unstage: unstage-image
	rm -f $(STAGED_INI)

# Give `sim` (from Makefile.common) the state its mode expects, so plain `make sim` works
# like every other test here. The erased mode no longer has to delete the image first --
# its config simply does not name one.
ifeq ($(STAGED),1)
sim: $(STAGED_INI) $(STAGED_IMG)
else
sim: $(NO_SPIPRELOAD_INI)
endif

clean-mode:
	rm -f .staged-mode

clean: unstage clean-mode

distclean: clean
	rm -f flash_fixture.bin
