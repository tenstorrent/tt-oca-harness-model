# SPDX-License-Identifier: Apache-2.0
# sw/smu-vp-tests/smu-aou-delay-test/Makefile
#
# AOU bridge-delay test: this test's smc.ini sets dut.aou.bridge_delay_ns and
# dut.clint.tick_period_ns in a [double] section, and the SMC firmware checks
# that MTIME advances by at least the configured delay per AOU hop.  Guards
# smu-vp's INI parser accepting [double] (smc-vp already does).  Run on smu-vp
# via sw/smu-vp-tests/run_smu_vp_tests.sh (or `make sim`).

VP        ?= ../../../vp/build_smc/bin/smu-vp
BASE_SMC_INI ?= ../../../vp/platform/smu/config/smc_smu_vp.ini
SEP_INI   ?= ../../../vp/platform/smu/config/sep_smu_config.ini
SIM_TIME_MS ?= 50

# Keep in sync with BRIDGE_DELAY_NS / MTIME_TICK_NS in smc_main.c.
BRIDGE_DELAY_NS ?= 10000
MTIME_TICK_NS   ?= 100

.PHONY: all smc sep sim clean

all: smc sep smc.ini

smc:
	$(MAKE) -f smc.mk

sep:
	$(MAKE) -f sep.mk

smc.ini: $(BASE_SMC_INI) Makefile
	cp $< $@
	printf '\n[double]\ndut.aou.bridge_delay_ns : %s\ndut.aou_peer.bridge_delay_ns : %s\ndut.clint.tick_period_ns : %s\n' \
		$(BRIDGE_DELAY_NS) $(BRIDGE_DELAY_NS) $(MTIME_TICK_NS) >> $@

sim: all
	$(VP) smc.ini smu_aou_delay_smc $(SEP_INI) smu_aou_delay_sep $(SIM_TIME_MS)

clean:
	$(MAKE) -f smc.mk clean
	$(MAKE) -f sep.mk clean
	rm -f smc.ini
