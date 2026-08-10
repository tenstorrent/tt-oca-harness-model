TEST = sep_aes_all_modes_test
export TEST

# Use the common SEP test build rules
# Force compile without Zb* bit-manip extensions to avoid illegal-instruction traps
EXT_C_FLAGS += -march=rv32imc -mabi=ilp32

# Include RDL-generated register headers and common macros
EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

all: $(TEST).elf

.PHONY: help
help:
	@echo "AES Multi-Mode Operation Test (FUNC-002)"
	@echo "Tests all 5 cipher modes: ECB, CBC, CFB, OFB, CTR"
	@echo "Targets:"
	@echo "  all  - Build $(TEST)"
	@echo "  clean- Clean artifacts"

.PHONY: clean
clean:
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map