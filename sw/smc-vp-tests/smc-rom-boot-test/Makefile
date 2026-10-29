# SPDX-License-Identifier: Apache-2.0
# sw/smc-vp-tests/smc-rom-boot-test/Makefile
#
# ROM-only boot: smc-vp is given `none` as the ELF, so the cores start at the boot ROM base
# and run rom.S, loaded through dut.bootrom.init_file. The ini is the platform's with that
# path filled in; smc-vp resolves it from its own working directory, so it is absolute.
SRCS   = rom.S
OBJS   = rom.o
TARGET = smc_rom_boot.elf
LDFLAGS += -T rom.ld

ROM_IMAGE = smc_rom_boot.bin
ROM_INI   = smc_rom_boot.ini
BASE_INI ?= ../../../vp/platform/smc/config/smc_platform_vp.ini
VP_FLAGS  = $(ROM_INI)
SIM_ELF   = none

include ../Makefile.common

all: $(ROM_IMAGE) $(ROM_INI)
sim: $(ROM_IMAGE) $(ROM_INI)

$(ROM_IMAGE): $(TARGET)
	$(OBJCOPY) -O binary $< $@

$(ROM_INI): $(BASE_INI)
	sed 's#^dut.bootrom.init_file *:.*#dut.bootrom.init_file : $(CURDIR)/$(ROM_IMAGE)#' $< > $@
	grep -q '^dut.bootrom.init_file : /' $@

clean: clean-rom
clean-rom:
	rm -f $(ROM_IMAGE) $(ROM_INI)
.PHONY: clean-rom
