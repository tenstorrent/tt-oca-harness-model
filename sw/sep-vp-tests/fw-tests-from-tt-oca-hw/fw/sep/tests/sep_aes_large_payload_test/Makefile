TEST = sep_aes_large_payload_test
export TEST

# Force compile without Zb* bit-manip extensions to avoid illegal-instruction traps
EXT_C_FLAGS += -march=rv32imc -mabi=ilp32

# Optional: raise the DCCM ciphertext-buffer ceiling (default 4096 blocks = 64 KiB).
# Keep within DCCM (128 KiB) headroom for stack/data; ~6144 blocks is a safe max.
# EXT_C_FLAGS += -DAES_LARGE_PAYLOAD_MAX_BLOCKS=6144

EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

all: $(TEST).elf

.PHONY: help
help:
	@echo "AES Large-Payload Random-Mode Test (STRESS-002)"
	@echo "Random mode/key (cocotb-injected seed) + encrypt/decrypt round-trip"
	@echo "Targets:"
	@echo "  all  - Build $(TEST)"
	@echo "  clean- Clean artifacts"

.PHONY: clean
clean:
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map
