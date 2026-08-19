TEST = sep_aes_mb_stream_test
export TEST

# Force compile without Zb* bit-manip extensions to avoid illegal-instruction traps
EXT_C_FLAGS += -march=rv32imc -mabi=ilp32

# Optional overrides:
#   chunk buffer size (default 256 blocks = 4 KiB pt + 4 KiB ct in DCCM)
# EXT_C_FLAGS += -DAES_MB_STREAM_CHUNK_BLOCKS=256
#   default total chunks when cocotb injects 0 (default 256 chunks = 1 MiB)
# EXT_C_FLAGS += -DAES_MB_STREAM_DEFAULT_CHUNKS=256

EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

all: $(TEST).elf

.PHONY: help
help:
	@echo "AES MB-Scale Streaming Round-Trip Test (STRESS-003)"
	@echo "Chunked ECB encrypt/decrypt round-trip; MB-scale payload, ~8 KiB DCCM"
	@echo "Targets:"
	@echo "  all  - Build $(TEST)"
	@echo "  clean- Clean artifacts"

.PHONY: clean
clean:
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map
