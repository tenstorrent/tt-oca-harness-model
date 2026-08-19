TEST = fabric_filter_wrap_edge_case_test
export TEST

OCH_ROOT ?= /proj_soc/user_dev/minshaoho/tryrun/dv_sep_d2

EXT_C_FLAGS += -march=rv32imc -mabi=ilp32

EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

all: $(TEST).elf

.PHONY: help
help:
	@echo "Fabric Filter Wrap Edge Case Test: $(TEST)"
	@echo "TC_FABRIC_070: axi_filter_wrap 79.75%% -> 90%%+ (需要 10.25%% 改進)"
	@echo "Strategy: Filter wrap邊界條件和異常case精準測試"
	@echo "Targets: all, clean"

.PHONY: clean
clean:
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map