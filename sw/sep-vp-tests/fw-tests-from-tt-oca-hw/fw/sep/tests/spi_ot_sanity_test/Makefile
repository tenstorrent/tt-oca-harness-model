TEST = spi_ot_sanity_test
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
	@echo "SPI OT Sanity Test - SPI mux selection and controller enable/disable"
	@echo "Targets:"
	@echo "  all  - Build $(TEST)"
	@echo "  clean- Clean artifacts"
	@echo ""
	@echo "Usage:"
	@echo "  make all"

.PHONY: clean
clean:
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map
