# SPDX-License-Identifier: Apache-2.0
# sw/smu-vp-tests/smu-rom-boot-test/Makefile
#
# SMU ROM-only boot test: the SMC runs only a boot ROM image (sw/smc-vp-tests/
# smc-rom-boot-test/rom.S), so smu-vp gets `none` as the SMC ELF and this test's smc.ini,
# which points dut.bootrom.init_file at the image. Run on smu-vp via
# sw/smu-vp-tests/run_smu_vp_tests.sh (or `make sim`).

VP        ?= ../../../vp/build_smc/bin/smu-vp
BASE_SMC_INI ?= ../../../vp/platform/smu/config/smc_smu_vp.ini
SEP_INI   ?= ../../../vp/platform/smu/config/sep_smu_config.ini
SIM_TIME_MS ?= 50

ROM_IMAGE = smu_rom_boot.bin

.PHONY: all rom sep sim clean

all: rom sep smc.ini

rom:
	$(MAKE) -f rom.mk

sep:
	$(MAKE) -f sep.mk

# smu-vp resolves init_file from the SEP ini's directory, so the path is absolute.
smc.ini: $(BASE_SMC_INI)
	sed 's#^dut.bootrom.init_file *:.*#dut.bootrom.init_file : $(CURDIR)/$(ROM_IMAGE)#' $< > $@
	grep -q '^dut.bootrom.init_file : /' $@

sim: all
	$(VP) smc.ini none $(SEP_INI) smu_rom_boot_sep $(SIM_TIME_MS)

clean:
	$(MAKE) -f rom.mk clean
	$(MAKE) -f sep.mk clean
	rm -f smc.ini
