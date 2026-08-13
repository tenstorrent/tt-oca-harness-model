# SPDX-License-Identifier: Apache-2.0
# sw/smu-vp-tests/smu-traffic-test/Makefile
#
# Port of tt-oca-hw dv/smu smc_cpu_traffic_sep_axi_test (50-beat burst).

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
	$(VP) $(SMC_INI) smu_traffic_smc $(SEP_INI) smu_traffic_sep $(SIM_TIME_MS)

clean:
	$(MAKE) -f smc.mk clean
	$(MAKE) -f sep.mk clean
