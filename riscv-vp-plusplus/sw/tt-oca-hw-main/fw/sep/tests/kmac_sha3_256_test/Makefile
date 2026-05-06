TEST = kmac_sha3_256_test
export TEST

EXT_C_FLAGS += -march=rv32imc -mabi=ilp32

EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common

all: $(VECTORS_HEADER) $(TEST).elf

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

# Auto-generate test vectors from Python hashlib (prevents typos in expected values)
VECTORS_HEADER = kmac_test_vectors.h
VECTORS_SCRIPT = generate_test_vectors.py

$(VECTORS_HEADER): $(VECTORS_SCRIPT)
	@echo "Generating test vectors..."
	python3 $(VECTORS_SCRIPT) $(VECTORS_HEADER)

# Test depends on auto-generated vectors
$(TEST).o: $(VECTORS_HEADER)



.PHONY: generate-test-vectors
generate-test-vectors:
	python3 $(VECTORS_SCRIPT) $(VECTORS_HEADER)

.PHONY: help
help:
	@echo "KMAC Test: $(TEST)"
	@echo "Targets: all, clean"

.PHONY: clean
clean:
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map
	rm -f $(VECTORS_HEADER)
