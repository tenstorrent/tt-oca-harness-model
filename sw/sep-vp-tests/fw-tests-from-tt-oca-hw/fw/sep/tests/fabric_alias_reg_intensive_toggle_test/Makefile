TEST = fabric_alias_reg_intensive_toggle_test
export TEST

OCH_ROOT ?= /proj_soc/user_dev/minshaoho/tryrun/dv_sep_d2

EXT_C_FLAGS += -march=rv32imc -mabi=ilp32

EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

all: $(TEST).elf

.PHONY: help
help:
	@echo "Fabric Alias Reg Intensive Toggle Test: $(TEST)"
	@echo "TC_FABRIC_069: 82.81%% -> 90%%+ (需要 7.19%% 改進)"
	@echo "Strategy: 16個別名條目的深度 CSR 欄位 toggle 強化"
	@echo "Targets: all, clean"

.PHONY: clean
clean:
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map