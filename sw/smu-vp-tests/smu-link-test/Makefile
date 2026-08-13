# SPDX-License-Identifier: Apache-2.0
# sw/smu-vp-tests/smu-link-test/Makefile
#
# SMU on-die link test: builds both firmware halves (SMC RV64 + SEP RV32)
# with their respective per-platform flags.  Run on smu-vp via
# sw/smu-vp-tests/run_smu_vp_tests.sh (or `make sim`).

VP        ?= ../../../vp/build_smc/bin/smu-vp
SMC_INI   ?= ../../../vp/platform/smu/config/smc_smu_vp.ini
SEP_INI   ?= ../../../vp/platform/smu/config/sep_smu_config.ini
SIM_TIME_MS ?= 50

.PHONY: all smc sep sim clean

all: smc sep

smc:
	$(MAKE) -f smc.mk

sep:
	$(MAKE) -f sep.mk

# Both firmwares print PASS/FAIL; the runner script greps for them.
sim: all
	$(VP) $(SMC_INI) smu_link_smc $(SEP_INI) smu_link_sep $(SIM_TIME_MS)

clean:
	$(MAKE) -f smc.mk clean
	$(MAKE) -f sep.mk clean
