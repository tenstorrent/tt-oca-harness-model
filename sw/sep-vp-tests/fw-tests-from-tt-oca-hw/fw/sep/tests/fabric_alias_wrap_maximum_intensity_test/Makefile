TEST = fabric_alias_wrap_maximum_intensity_test
export TEST

OCH_ROOT ?= /proj_soc/user_dev/minshaoho/tryrun/dv_sep_d2

EXT_C_FLAGS += -march=rv32imc -mabi=ilp32

EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

all: $(TEST).elf

.PHONY: help
help:
	@echo "Fabric Alias Wrap Maximum Intensity Test: $(TEST)"
	@echo "TC_FABRIC_072: axi_alias_remap_wrap 39.13%% -> 90%%+ (需要 50.87%% 改進) [最關鍵]"
	@echo "Strategy: 最大強度alias wrap測試，完整模組激活"
	@echo "Targets: all, clean"

.PHONY: clean
clean:
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map