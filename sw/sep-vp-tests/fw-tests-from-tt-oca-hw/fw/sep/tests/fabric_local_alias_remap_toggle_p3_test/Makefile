TEST = fabric_local_alias_remap_toggle_p3_test
export TEST

OCH_ROOT ?= /proj_soc/user_dev/minshaoho/tryrun/dv_sep_d2

EXT_C_FLAGS += -march=rv32imc -mabi=ilp32

EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

all: $(TEST).elf

.PHONY: help
help:
	@echo "Fabric Local Alias Remap Toggle P3 Test: $(TEST)"
	@echo "TC_FABRIC_064: axi_window_remap 64.33%% -> 90%%+ (需要 25.67%% 改進)"
	@echo "Strategy: Local別名重映射toggle強化，完整CSR欄位覆蓋"
	@echo "Targets: all, clean"

.PHONY: clean
clean:
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map