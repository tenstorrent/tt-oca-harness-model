TEST = dma_hash_test
export TEST

# Use the common SEP test build rules
# Force compile without Zb* bit-manip extensions to avoid illegal-instruction traps
EXT_C_FLAGS += -march=rv32imc -mabi=ilp32

# Include RDL-generated register headers and common macros
EXT_INCS += -I$(OCH_ROOT)/meta/registers/c
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common
EXT_INCS += -I$(OCH_ROOT)/fw/sep/tests/common/sha256

# SHA256 library source
SHA256_DIR = $(OCH_ROOT)/fw/sep/tests/common/sha256
SHA256_SRCS = $(SHA256_DIR)/sha256.c
SHA256_OBJS = $(SHA256_SRCS:.c=.o)

# Interrupt utilities source
COMMON_DIR = $(OCH_ROOT)/fw/sep/tests/common
INTERRUPT_SRCS = $(COMMON_DIR)/interrupt.c
INTERRUPT_OBJS = $(INTERRUPT_SRCS:.c=.o)

include $(OCH_ROOT)/fw/sep/tests/common/common.mk

# Custom build rule to include SHA256 and interrupt objects
$(TEST).elf: $(OBJS) $(SHA256_OBJS) $(INTERRUPT_OBJS) $(COMMON_OBJS) picolibc ${SNAPSHOT_DIR}/defines.h
	@echo Building $(TEST).elf with SHA256 and interrupt libraries
	$(GCC_PREFIX)-gcc $(LD_ABI) $(LD_FLAGS) $(OBJS) $(SHA256_OBJS) $(INTERRUPT_OBJS) $(COMMON_OBJS) -o $(TEST).elf
	$(GCC_PREFIX)-objdump -S -t -j .data -j .text $(TEST).elf > $(TEST).dis
	$(GCC_PREFIX)-nm -B -n $(TEST).elf > $(TEST).sym
	$(GCC_PREFIX)-objcopy -O verilog $(TEST).elf $(ITCM_ELF_SECTIONS) --change-addresses "-$(ITCM_START_ADDR)" $(TEST).itcm.hex
	$(GCC_PREFIX)-objcopy -O verilog $(TEST).elf $(DTCM_ELF_SECTIONS) --change-addresses "-$(DTCM_START_ADDR)" $(TEST).dtcm.hex
	@echo Completed building $(TEST)

all: $(TEST).elf

# Compilation rule for SHA256 library
$(SHA256_DIR)/%.o : $(SHA256_DIR)/%.c picolibc ${SNAPSHOT_DIR}/defines.h
	$(GCC_PREFIX)-gcc ${INCS} ${C_FLAGS} -c $< -o $@

# Compilation rule for interrupt utilities
$(COMMON_DIR)/%.o : $(COMMON_DIR)/%.c picolibc ${SNAPSHOT_DIR}/defines.h
	$(GCC_PREFIX)-gcc ${INCS} ${C_FLAGS} -c $< -o $@
