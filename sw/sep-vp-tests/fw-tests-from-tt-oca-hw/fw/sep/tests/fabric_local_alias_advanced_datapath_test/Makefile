TEST = fabric_local_alias_advanced_datapath_test
export TEST

OCH_ROOT ?= /proj_soc/user_dev/minshaoho/tryrun/dv_sep_d2

EXT_C_FLAGS += -march=rv32imc -mabi=ilp32

EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

all: $(TEST).elf

.PHONY: help
help:
	@echo "Fabric Local Alias Advanced Datapath Test: $(TEST)"
	@echo "TC_FABRIC_071: axi_window_remap 72.95%% -> 90%%+ (需要 17.05%% 改進)"
	@echo "Strategy: Local別名進階datapath，深度信號覆蓋"
	@echo "Targets: all, clean"

.PHONY: clean
clean:
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map