TEST = spi_ot_dma_tx_test
export TEST

# Force compile without Zb* bit-manip extensions to avoid illegal-instruction traps
# The VeeR EL2 core in simulation may not have these extensions enabled
EXT_C_FLAGS += -march=rv32imc -mabi=ilp32

# Include RDL-generated register headers (och_sep_top_reg.h)
EXT_INCS = -I$(OCH_ROOT)/meta/registers/c

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

all: $(TEST).elf

.PHONY: help
help:
	@echo "SPI OT DMA TX Test - SRAM-to-SPI TX FIFO via DMA hardware handshake"
	@echo "Targets:"
	@echo "  all  - Build $(TEST)"
	@echo "  clean- Clean artifacts"
	@echo ""
	@echo "Usage:"
	@echo "  make all"
	@echo "  sep_run sep_spi_ot_dma_tx_test"

.PHONY: clean
clean:
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map
