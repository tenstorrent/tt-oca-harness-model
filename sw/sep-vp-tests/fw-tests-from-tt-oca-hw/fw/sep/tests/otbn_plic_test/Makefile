# OTBN Interrupt to PLIC Test (INT-002, P1)
# Verifies OTBN done interrupt routing to PIC

TEST = otbn_plic_test
export TEST

.DEFAULT_GOAL := all

ifndef OCH_ROOT
$(error OCH_ROOT is not set. Please run: export OCH_ROOT=<your_och_path>)
endif

ifndef RV_ROOT
$(error RV_ROOT is not set. Please run: export RV_ROOT=$${OCH_ROOT}/deps/el2)
endif

EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common

# Interrupt utilities source
COMMON_DIR = $(OCH_ROOT)/fw/sep/tests/common
INTERRUPT_SRCS = $(COMMON_DIR)/interrupt.c
INTERRUPT_OBJS = $(INTERRUPT_SRCS:.c=.o)

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

$(TEST).elf: $(TEST).o $(INTERRUPT_OBJS) $(COMMON_OBJS) picolibc ${SNAPSHOT_DIR}/defines.h
	@echo "Building $(TEST).elf"
	$(GCC_PREFIX)-gcc $(LD_ABI) $(LD_FLAGS) $(TEST).o $(INTERRUPT_OBJS) $(COMMON_OBJS) -o $(TEST).elf
	$(GCC_PREFIX)-objdump -S -j .data -j .text $(TEST).elf > $(TEST).dis
	$(GCC_PREFIX)-nm -B -n $(TEST).elf > $(TEST).sym
	$(GCC_PREFIX)-objcopy -O verilog $(TEST).elf $(ITCM_ELF_SECTIONS) --change-addresses "-$(ITCM_START_ADDR)" $(TEST).itcm.hex
	$(GCC_PREFIX)-objcopy -O verilog $(TEST).elf $(DTCM_ELF_SECTIONS) --change-addresses "-$(DTCM_START_ADDR)" $(TEST).dtcm.hex
	@echo "Completed building $(TEST)"

.PHONY: clean
clean:
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map

all: $(TEST).elf
