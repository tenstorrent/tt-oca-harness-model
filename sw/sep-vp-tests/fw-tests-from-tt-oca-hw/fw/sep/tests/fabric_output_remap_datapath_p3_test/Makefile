TEST = fabric_output_remap_datapath_p3_test
export TEST

OCH_ROOT ?= /proj_soc/user_dev/minshaoho/tryrun/dv_sep_d2

EXT_C_FLAGS += -march=rv32imc -mabi=ilp32

EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

all: $(TEST).elf

.PHONY: help
help:
	@echo "Fabric Output Remap Datapath P3 Test: $(TEST)"
	@echo "TC_FABRIC_063: output_remap 64.35%% -> 90%%+, output_remap_reg 86.06%% -> 90%%+"
	@echo "Strategy: AP/STEE remap 16區域完整datapath矩陣"
	@echo "Targets: all, clean"

.PHONY: clean
clean:
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map