TEST = hmac_wipe_secret_test
export TEST

EXT_C_FLAGS += -march=rv32imc -mabi=ilp32

EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

all: $(TEST).elf

.PHONY: help
help:
	@echo "HMAC Test: $(TEST)"
	@echo "Targets: all, clean"

.PHONY: clean
clean:
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map
