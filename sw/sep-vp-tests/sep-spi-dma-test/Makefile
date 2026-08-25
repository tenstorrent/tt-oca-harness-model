# SPI-controller -> secure-DMA streaming integration test Makefile

SRCS = ../common/start.S main.c ../common/printf.c
OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)
TARGET = main
LDFLAGS += -T ../common/link.ld

include ../Makefile.common

# --- Dedicated VP config + flash fixture -----------------------------------
# This test verifies DMA-drained bytes against an image in the SPI flash, so it needs the
# bootcode preload out of the way (see NO_SPIPRELOAD_INI in ../Makefile.common). It derives
# its own config rather than sharing that one, because the config is what names the image:
# the flash model only loads what it is told to, so a test that stages nothing is
# guaranteed erased flash no matter what another test left in the directory. The filename
# is test-specific for the same reason -- two tests staging different fixtures must not
# land on the same path. Paths in the config are relative to it, and sep-vp chdir's there.
TEST_INI    := $(dir $(BASE_INI))accellera_config_spi_dma_test.ini
FIXTURE_REL := data/sep_spi_dma_test_flash.bin
STAGED_IMG  := $(dir $(BASE_INI))$(FIXTURE_REL)
VP_FLAGS    := $(TEST_INI)

.PHONY: fixture stage unstage distclean

$(TEST_INI): $(BASE_INI)
	sed 's|^och_sep_ss1\.spiPreload|#&|' $< > $@
	echo 'och_sep_ss1.spiBackdoorFile : "$(FIXTURE_REL)"' >> $@

flash_fixture.bin: gen_flash_fixture.py
	python3 gen_flash_fixture.py $@

fixture: flash_fixture.bin

$(STAGED_IMG): flash_fixture.bin
	mkdir -p $(dir $@) && cp $< $@

stage: $(STAGED_IMG)

unstage:
	rm -f $(STAGED_IMG) $(TEST_INI)
	-rmdir $(dir $(STAGED_IMG)) 2>/dev/null || true

# Give `sim` (from Makefile.common) the config and fixture as prerequisites so plain
# `make sim` works like every other test here. Without the fixture every read compares
# against an empty flash and all four transfers fail.
sim: $(TEST_INI) $(STAGED_IMG)

clean: unstage

distclean: clean
	rm -f flash_fixture.bin
