TEST = fabric_filter_datapath_matrix_p3_test
export TEST

OCH_ROOT ?= /proj_soc/user_dev/minshaoho/tryrun/dv_sep_d2

EXT_C_FLAGS += -march=rv32imc -mabi=ilp32

EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

all: $(TEST).elf

.PHONY: help
help:
	@echo "Fabric Filter Datapath Matrix P3 Test: $(TEST)"
	@echo "TC_FABRIC_061: traffic_filter 88.11%% -> 90%%+, axi_filter_wrap 64.11%% -> 90%%+"
	@echo "Strategy: 針對性的 pass/block 流量測試"
	@echo "Targets: all, clean"

.PHONY: clean
clean:
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map