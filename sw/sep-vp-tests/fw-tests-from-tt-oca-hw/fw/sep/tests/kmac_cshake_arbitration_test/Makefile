TEST = kmac_cshake_arbitration_test
export TEST

EXT_C_FLAGS += -march=rv32imc -mabi=ilp32
EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

all: $(TEST).elf

.PHONY: help
help:
	@echo "KMAC cSHAKE back-to-back arbitration test (TC_KMAC_016, P1)"
	@echo "4 sequential cSHAKE-128 ops: determinism and distinctness checks"

.PHONY: clean
clean:
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map
