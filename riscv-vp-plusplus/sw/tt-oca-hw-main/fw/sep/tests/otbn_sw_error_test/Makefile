# OTBN Software Error Handling Test
# Exercises recoverable and fatal OTBN software error paths from SEP firmware.

TEST = otbn_sw_error_test
export TEST

.DEFAULT_GOAL := all

ifndef OCH_ROOT
$(error OCH_ROOT is not set. Please run: export OCH_ROOT=<your_och_path>)
endif

ifndef RV_ROOT
$(error RV_ROOT is not set. Please run: export RV_ROOT=$${OCH_ROOT}/deps/el2)
endif

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

OTBN_APP_MK = $(OCH_ROOT)/fw/sep/tests/common_otbn/otbn_app.mk
OTBN_SRC_DIR = otbn_src
OTBN_BUILD_ROOT = otbn_build
OTBN_ERROR_APPS = bad_data_addr bad_insn_addr illegal_insn call_stack loop_error

OTBN_GENERATED_C_FILES = $(foreach app,$(OTBN_ERROR_APPS),$(OTBN_BUILD_ROOT)/$(app)/$(app)_otbn.c)
OTBN_GENERATED_H_FILES = $(foreach app,$(OTBN_ERROR_APPS),$(OTBN_BUILD_ROOT)/$(app)/$(app)_otbn.h)
OTBN_GENERATED_OBJS = $(OTBN_GENERATED_C_FILES:.c=.o)

EXT_INCS += $(foreach app,$(OTBN_ERROR_APPS),-I$(OTBN_BUILD_ROOT)/$(app))

define OTBN_APP_RULES
$(OTBN_BUILD_ROOT)/$(1)/$(1)_otbn.c $(OTBN_BUILD_ROOT)/$(1)/$(1)_otbn.h:
	$$(MAKE) -f $$(OTBN_APP_MK) \
		OCH_ROOT=$$(OCH_ROOT) \
		OTBN_APP_NAME=$(1) \
		OTBN_SRC_DIR=$$(OTBN_SRC_DIR) \
		OTBN_APP_SRCS=$$(OTBN_SRC_DIR)/$(1).s \
		OTBN_BUILD_DIR=$$(OTBN_BUILD_ROOT)/$(1) \
		otbn-app
endef

$(foreach app,$(OTBN_ERROR_APPS),$(eval $(call OTBN_APP_RULES,$(app))))

OTBN_C_FLAGS = -Os -fdata-sections -ffunction-sections -fno-common $(INCS) $(CC_ABI) $(EXT_OTBN_C_FLAGS)

$(OTBN_BUILD_ROOT)/%/%_otbn.o: $(OTBN_BUILD_ROOT)/%/%_otbn.c $(OTBN_BUILD_ROOT)/%/%_otbn.h
	@echo "Compiling OTBN-generated C file (no stack usage): $<"
	$(GCC_PREFIX)-gcc $(OTBN_C_FLAGS) -c $< -o $@

$(TEST).o: $(TEST).c $(OTBN_GENERATED_H_FILES)

$(TEST).elf: $(TEST).o $(OTBN_GENERATED_OBJS) $(COMMON_OBJS) picolibc ${SNAPSHOT_DIR}/defines.h
	@echo "Building $(TEST).elf"
	$(GCC_PREFIX)-gcc $(LD_ABI) $(LD_FLAGS) $(TEST).o $(OTBN_GENERATED_OBJS) $(COMMON_OBJS) -o $(TEST).elf
	$(GCC_PREFIX)-objdump -S -j .data -j .text $(TEST).elf > $(TEST).dis
	$(GCC_PREFIX)-nm -B -n $(TEST).elf > $(TEST).sym
	$(GCC_PREFIX)-objcopy -O verilog $(TEST).elf $(ITCM_ELF_SECTIONS) --change-addresses "-$(ITCM_START_ADDR)" $(TEST).itcm.hex
	$(GCC_PREFIX)-objcopy -O verilog $(TEST).elf $(DTCM_ELF_SECTIONS) --change-addresses "-$(DTCM_START_ADDR)" $(TEST).dtcm.hex
	@echo "Completed building $(TEST)"

.PHONY: clean otbn-apps

otbn-apps: $(OTBN_GENERATED_C_FILES) $(OTBN_GENERATED_H_FILES)

all: otbn-apps $(TEST).elf

clean:
	rm -rf $(OTBN_BUILD_ROOT)
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map
