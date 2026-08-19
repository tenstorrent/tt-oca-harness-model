TEST = fabric_alias_remap_datapath_p3_test
export TEST

OCH_ROOT ?= /proj_soc/user_dev/minshaoho/tryrun/dv_sep_d2

EXT_C_FLAGS += -march=rv32imc -mabi=ilp32

EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

all: $(TEST).elf

.PHONY: help
help:
	@echo "Fabric Alias Remap Datapath P3 Test: $(TEST)"
	@echo "TC_FABRIC_062: axi_alias_remap_wrap 2.18%% -> 90%%+ [最關鍵]"
	@echo "Strategy: Local-master 別名 hit/miss/boundary 完整AXI datapath覆蓋"
	@echo "Targets: all, clean"

.PHONY: clean
clean:
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map