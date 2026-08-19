TEST = fabric_output_remap_micro_optimization_test
export TEST

OCH_ROOT ?= /proj_soc/user_dev/minshaoho/tryrun/dv_sep_d2

EXT_C_FLAGS += -march=rv32imc -mabi=ilp32

EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

all: $(TEST).elf

.PHONY: help
help:
	@echo "Fabric Output Remap Micro-Optimization Test: $(TEST)"
	@echo "TC_FABRIC_067: 89.83%% -> 90%%+ (需要僅 0.17%% 改進)"
	@echo "Focus: 極小量邊界條件補強，專注最後幾個未觸及的 toggle 位元"
	@echo "Targets: all, clean"

.PHONY: clean
clean:
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map