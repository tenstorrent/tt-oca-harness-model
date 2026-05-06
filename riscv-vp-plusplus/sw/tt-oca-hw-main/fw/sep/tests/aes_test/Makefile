TEST = aes_test
export TEST

# Use the common SEP test build rules
# Force compile without Zb* bit-manip extensions to avoid illegal-instruction traps
EXT_C_FLAGS += -march=rv32imc -mabi=ilp32

# Include RDL-generated register headers and common macros
EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common

all: $(TEST).elf

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

.PHONY: help
help:
	@echo "AES encryption/decryption test (OpenTitan guide aligned)"
	@echo "Targets:"
	@echo "  all  - Build $(TEST)"
	@echo "  clean- Clean artifacts"

.PHONY: clean
clean:
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map
