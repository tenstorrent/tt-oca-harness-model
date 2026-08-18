TEST = sep_cpu_sram_aes_sram_test
export TEST

EXT_C_FLAGS += -march=rv32imc -mabi=ilp32
EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

all: $(TEST).elf

.PHONY: help
help:
	@echo "CPU SRAM-AES-SRAM integration test (#565)"
	@echo "Real EL2 CPU + production KM sideload + AES + two SRAM regions"

.PHONY: clean
clean:
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map
