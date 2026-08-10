TEST = spi_xspi_dma_test
export TEST

# Force compile without Zb* bit-manip extensions to avoid illegal-instruction traps
# The VeeR EL2 core in simulation may not have these extensions enabled
EXT_C_FLAGS += -march=rv32imc -mabi=ilp32

# Include RDL-generated register headers (och_sep_top_reg.h)
EXT_INCS = -I$(OCH_ROOT)/meta/registers/c

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

# OT SPI mode variant (same source compiled with -DUSE_OT_SPI)
OT_TEST = spi_xspi_dma_ot_mode

all: $(TEST).elf ot

.PHONY: ot
ot: $(OT_TEST).elf

$(OT_TEST).o: $(TEST).c picolibc ${SNAPSHOT_DIR}/defines.h
	$(GCC_PREFIX)-gcc $(INCS) $(C_FLAGS) -DUSE_OT_SPI -DCOMPILER_FLAGS="\"$(C_FLAGS)\"" -c $< -o $@

$(OT_TEST).elf: $(OT_TEST).o $(COMMON_OBJS) picolibc ${SNAPSHOT_DIR}/defines.h
	$(GCC_PREFIX)-gcc $(LD_ABI) $(LD_FLAGS) $(OT_TEST).o $(COMMON_OBJS) -o $(OT_TEST).elf
	$(GCC_PREFIX)-objdump -S -t -j .data -j .text $(OT_TEST).elf > $(OT_TEST).dis
	$(GCC_PREFIX)-nm -B -n $(OT_TEST).elf > $(OT_TEST).sym
	$(GCC_PREFIX)-objcopy -O verilog $(OT_TEST).elf $(ITCM_ELF_SECTIONS) --change-addresses "-$(ITCM_START_ADDR)" $(OT_TEST).itcm.hex
	$(GCC_PREFIX)-objcopy -O verilog $(OT_TEST).elf $(DTCM_ELF_SECTIONS) --change-addresses "-$(DTCM_START_ADDR)" $(OT_TEST).dtcm.hex
	@echo Completed building $(OT_TEST)

.PHONY: help
help:
	@echo "TC_SPI_CDN_005: Cadence xSPI DMA Write/Read Test"
	@echo ""
	@echo "Targets:"
	@echo "  all  - Build $(TEST)"
	@echo "  clean- Clean artifacts"
	@echo ""
	@echo "Usage:"
	@echo "  make all"
	@echo "  sep_run sep_spi_xspi_dma_test"

.PHONY: clean
clean:
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map
