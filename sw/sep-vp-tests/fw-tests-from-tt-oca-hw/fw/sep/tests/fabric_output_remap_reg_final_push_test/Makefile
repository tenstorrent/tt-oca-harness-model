TEST = fabric_output_remap_reg_final_push_test
export TEST

OCH_ROOT ?= /proj_soc/user_dev/minshaoho/tryrun/dv_sep_d2

EXT_C_FLAGS += -march=rv32imc -mabi=ilp32

EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

all: $(TEST).elf

.PHONY: help
help:
	@echo "Fabric Output Remap Reg Final Push Test: $(TEST)"
	@echo "TC_FABRIC_068: 86.36%% -> 90%%+ (需要 3.64%% 改進)"
	@echo "Strategy: CSR 欄位剩餘 toggle 位元完整掃描"
	@echo "Targets: all, clean"

.PHONY: clean
clean:
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map