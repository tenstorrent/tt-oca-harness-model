# SPDX-License-Identifier: Apache-2.0
# sw/smu-vp-tests/smu-aou-ext-test/Makefile
#
# Port of tt-oca-hw smu_sep_ext_axi (SMC + SEP -> ext_out via AOU catch-all).

VP        ?= ../../../vp/build_smc/bin/smu-vp
SMC_INI   ?= ../../../vp/platform/smu/config/smc_smu_vp.ini
SEP_INI   ?= ../../../vp/platform/smu/config/sep_smu_config.ini
# SEP waits for the SMC doorbell, then programs outbound filter 0 and does
# an ext_out beat. 50 ms ends the run after the doorbell once Adams Bridge
# is on the SEP bus (extra reset/elaboration work).
SIM_TIME_MS ?= 200

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
