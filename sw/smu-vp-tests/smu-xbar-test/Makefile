# SPDX-License-Identifier: Apache-2.0
# sw/smu-vp-tests/smu-xbar-test/Makefile
#
# Port of tt-oca-hw smc_sep_xbar / smu_bidirect (handshake tokens).

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
	$(VP) $(SMC_INI) smu_xbar_smc $(SEP_INI) smu_xbar_sep $(SIM_TIME_MS)

clean:
	$(MAKE) -f smc.mk clean
	$(MAKE) -f sep.mk clean
