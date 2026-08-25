# SPDX-License-Identifier: Apache-2.0
# sw/smu-vp-tests/smu-mailbox-test/Makefile
#
# SEP mailbox -> SMC interrupt delivery over the SMU (RTL: sep.sv
# smc_mailbox_interrupt_o -> smu.sv -> smc_peripherals.sv peripheral bits 7:0).

VP        ?= ../../../vp/build/bin/smu-vp
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
	$(VP) $(SMC_INI) smu_mailbox_smc $(SEP_INI) smu_mailbox_sep $(SIM_TIME_MS)

clean:
	$(MAKE) -f smc.mk clean
	$(MAKE) -f sep.mk clean
