# OTBN Firmware Control Flow Test (SEP-002, P1)
# Exercises complete FW control of OTBN execution flow.

TEST = otbn_fw_control_test
export TEST

.DEFAULT_GOAL := all

ifndef OCH_ROOT
$(error OCH_ROOT is not set. Please run: export OCH_ROOT=<your_och_path>)
endif

ifndef RV_ROOT
$(error RV_ROOT is not set. Please run: export RV_ROOT=$${OCH_ROOT}/deps/el2)
endif

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

OTBN_APP_NAME = otbn_smoke
OTBN_SRC_DIR = $(OCH_ROOT)/fw/sep/tests/otbn_smoke_test/otbn_src
OTBN_APP_SRCS = $(OTBN_SRC_DIR)/otbn_smoke.s

include $(OCH_ROOT)/fw/sep/tests/common_otbn/otbn_app.mk

EXT_INCS += -I$(OTBN_BUILD_DIR)

OTBN_GENERATED_C = $(OTBN_APP_C_FILE)
OTBN_GENERATED_H = $(OTBN_APP_H_FILE)

$(TEST).o: $(TEST).c $(OTBN_GENERATED_H)

OTBN_C_FLAGS = -Os -fdata-sections -ffunction-sections -fno-common $(INCS) $(CC_ABI) $(EXT_OTBN_C_FLAGS)

$(OTBN_GENERATED_C:.c=.o): $(OTBN_GENERATED_C) $(OTBN_GENERATED_H)
	@echo "Compiling OTBN-generated C file (no stack usage): $(OTBN_GENERATED_C)"
	$(GCC_PREFIX)-gcc $(OTBN_C_FLAGS) -c $(OTBN_GENERATED_C) -o $@

$(TEST).elf: $(TEST).o $(OTBN_GENERATED_C:.c=.o) $(COMMON_OBJS) picolibc ${SNAPSHOT_DIR}/defines.h
	@echo "Building $(TEST).elf"
	$(GCC_PREFIX)-gcc $(LD_ABI) $(LD_FLAGS) $(TEST).o $(OTBN_GENERATED_C:.c=.o) $(COMMON_OBJS) -o $(TEST).elf
	$(GCC_PREFIX)-objdump -S -j .data -j .text $(TEST).elf > $(TEST).dis
	$(GCC_PREFIX)-nm -B -n $(TEST).elf > $(TEST).sym
	$(GCC_PREFIX)-objcopy -O verilog $(TEST).elf $(ITCM_ELF_SECTIONS) --change-addresses "-$(ITCM_START_ADDR)" $(TEST).itcm.hex
	$(GCC_PREFIX)-objcopy -O verilog $(TEST).elf $(DTCM_ELF_SECTIONS) --change-addresses "-$(DTCM_START_ADDR)" $(TEST).dtcm.hex
	@echo "Completed building $(TEST)"

.PHONY: clean
clean: otbn-app-clean
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map

all: otbn-app $(TEST).elf
