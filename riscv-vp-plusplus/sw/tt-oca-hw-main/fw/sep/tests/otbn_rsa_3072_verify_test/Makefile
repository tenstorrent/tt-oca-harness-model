# OTBN RSA-3072 Verification Test
# Sanity check for OTBN RSA integration using locally generated test vectors
# Ported from dv/sep/tests/ to fw/sep/tests/ conventions.

TEST = otbn_rsa_3072_verify_test
export TEST

# Default target
.DEFAULT_GOAL := all

# Check required environment variables
ifndef OCH_ROOT
$(error OCH_ROOT is not set. Please run: export OCH_ROOT=<your_och_path>)
endif

ifndef RV_ROOT
$(error RV_ROOT is not set. Please run: export RV_ROOT=$${OCH_ROOT}/deps/el2)
endif

# Add register header include paths
EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common

# Include the OCH common build rules
include $(OCH_ROOT)/fw/sep/tests/common/common.mk

# OTBN Application Configuration
OTBN_APP_NAME = rsa_3072_app
OTBN_SRC_DIR = otbn_src
# Explicit list of source files needed for our verification test (subset of available files)
OTBN_APP_SRCS = $(OTBN_SRC_DIR)/gcd.s $(OTBN_SRC_DIR)/modexp.s $(OTBN_SRC_DIR)/montmul.s \
				$(OTBN_SRC_DIR)/mul.s $(OTBN_SRC_DIR)/rsa_keygen.s $(OTBN_SRC_DIR)/rsa_modinv_f4.s \
				$(OTBN_SRC_DIR)/rsa_primality.s $(OTBN_SRC_DIR)/run_rsa_mem.s $(OTBN_SRC_DIR)/run_rsa.s

# Include the common OTBN build infrastructure
include $(OCH_ROOT)/fw/sep/tests/common_otbn/otbn_app.mk

# Add OTBN build directory to include path for generated headers
EXT_INCS += -I$(OTBN_BUILD_DIR)

# Generated OTBN files from common_otbn infrastructure (now in build directory)
OTBN_GENERATED_C = $(OTBN_APP_C_FILE)
OTBN_GENERATED_H = $(OTBN_APP_H_FILE)

# Test-specific dependencies
$(TEST).o: $(TEST).c $(OTBN_GENERATED_H)

# Compilation flags for OTBN-generated C files (no stack usage analysis)
# OTBN assembly doesn't use a stack, so stack usage analysis is meaningless
OTBN_C_FLAGS = -Os -fdata-sections -ffunction-sections -fno-common $(INCS) $(CC_ABI) $(EXT_OTBN_C_FLAGS)

# Custom compilation rule for OTBN-generated C file
$(OTBN_GENERATED_C:.c=.o): $(OTBN_GENERATED_C) $(OTBN_GENERATED_H)
	@echo "Compiling OTBN-generated C file (no stack usage): $(OTBN_GENERATED_C)"
	$(GCC_PREFIX)-gcc $(OTBN_C_FLAGS) -c $(OTBN_GENERATED_C) -o $@

# Custom .elf rule that includes the generated OTBN object file
# We need this because common.mk's OBJS uses $(wildcard *.c) which doesn't see generated files
$(TEST).elf: $(TEST).o $(OTBN_GENERATED_C:.c=.o) $(COMMON_OBJS) picolibc ${SNAPSHOT_DIR}/defines.h
	@echo "Building $(TEST).elf"
	$(GCC_PREFIX)-gcc $(LD_ABI) $(LD_FLAGS) $(TEST).o $(OTBN_GENERATED_C:.c=.o) $(COMMON_OBJS) -o $(TEST).elf
	$(GCC_PREFIX)-objdump -S -j .data -j .text $(TEST).elf > $(TEST).dis
	$(GCC_PREFIX)-nm -B -n $(TEST).elf > $(TEST).sym
	$(GCC_PREFIX)-objcopy -O verilog $(TEST).elf $(ITCM_ELF_SECTIONS) --change-addresses "-$(ITCM_START_ADDR)" $(TEST).itcm.hex
	$(GCC_PREFIX)-objcopy -O verilog $(TEST).elf $(DTCM_ELF_SECTIONS) --change-addresses "-$(DTCM_START_ADDR)" $(TEST).dtcm.hex
	@echo "Completed building $(TEST)"

# Override clean rule to include OTBN artifacts
.PHONY: clean
clean: otbn-app-clean
	rm -f *.o *.elf *.hex *.dis *.sym *.su *.map

# Main target depends on OTBN application
all: otbn-app $(TEST).elf
