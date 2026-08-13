# SPDX-License-Identifier: Apache-2.0
# sw/smu-vp-tests/smu-aou-ext-test/Makefile
#
# Port of tt-oca-hw smu_sep_ext_axi (SMC->ext_out via AOU catch-all).

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

sim: all
	$(VP) $(SMC_INI) smu_aou_ext_smc $(SEP_INI) smu_aou_ext_sep $(SIM_TIME_MS)

clean:
	$(MAKE) -f smc.mk clean
	$(MAKE) -f sep.mk clean
