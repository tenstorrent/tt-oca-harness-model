TEST = spi_ot_enable_disable_test
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
	@echo "OpenTitan SPI Host Sanity Test - Tests SPI controller initialization at 25 MHz"
	@echo "Targets:"
	@echo "  all  - Build $(TEST)"
	@echo "  clean- Clean artifacts"
	@echo ""
	@echo "Usage:"
	@echo "  make all"
	@echo "  sep_run sep_spi_ot_enable_disable_test_test"

.PHONY: clean
clean:
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map
