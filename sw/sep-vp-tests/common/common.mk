# TODO: Error out if RV_ROOT is not set
# TODO: Error out if TEST is not set

SEP_TESTS_COMMON_DIR = $(SEP_TESTS_DIR)/common/

MAKE ?= make
GCC_PREFIX ?= riscv64-unknown-elf

PICOLIBC_DIR ?= ${RV_ROOT}/third_party/picolibc/install

SNAPSHOT ?= sep
SNAPSHOT_DIR = ${RV_ROOT}/snapshots/${SNAPSHOT}

SEP_SIM_DIR = ${OCH_ROOT}/dv/sep/
SEP_TESTS_DIR = $(SEP_SIM_DIR)/tests

ABI ?= -mabi=ilp32
LD_ABI ?= $(ABI) -march=rv32imac
# LINKER_SCRIPT ?= $(SNAPSHOT_DIR)/link.ld
LINKER_SCRIPT ?= $(SEP_TESTS_DIR)/common/exec_from_tcms.ld
TEST_LIBS ?=

# Override march depending on used GCC version
ifneq ($(shell which $(GCC_PREFIX)-gcc 2> /dev/null),)
	GCCVERSIONGT11 := $(shell expr `$(GCC_PREFIX)-gcc -dumpversion | cut -f1 -d.` \>= 11)
	ifeq "$(GCCVERSIONGT11)" "1"
		CC_ABI = $(ABI) -march=rv32imc_zicsr_zifencei_zba_zbb_zbc_zbs
	else
		CC_ABI = $(ABI) -march=rv32imc
	endif
endif

EXT_INCS ?= -I$(OCH_ROOT)/hw/comp/uart_16550/data/registers/c # TODO: Move to appropriate location

INCS ?= -I$(SNAPSHOT_DIR) -isystem $(PICOLIBC_DIR)/picolibc/$(GCC_PREFIX)/include $(EXT_INCS)
LIBS ?= $(EXT_LIBS)

C_FLAGS = -Os -fdata-sections -ffunction-sections -fno-common -fstack-usage $(INCS) $(CC_ABI) $(EXT_C_FLAGS)
LD_FLAGS = -Wl,--gc-sections -Wl,-Map=$(TEST).map -T$(LINKER_SCRIPT) --specs=$(PICOLIBC_DIR)/picolibc.specs $(LIBS) -nostartfiles $(EXT_LD_FLAGS)

COMMON_C_SRCS = $(SEP_TESTS_DIR)/common/init_stdout.c
COMMON_ASM_SRCS = $(SEP_TESTS_DIR)/common/crt0.s
COMMON_OBJS = $(COMMON_C_SRCS:.c=.o) $(COMMON_ASM_SRCS:.s=.o)

C_SRCS ?= $(wildcard *.c)
ASM_SRCS ?= $(wildcard *.s)
OBJS = $(C_SRCS:.c=.o) $(ASM_SRCS:.s=.o)

# Determine ITCM and DTCM offset from config output (from "iccm_sadr" and "dccm_sadr" fields in perl_configs.pl)
EL2_CONFIG_FILE = $(SNAPSHOT_DIR)/perl_configs.pl
ITCM_START_ADDR = $(shell perl -e 'my $$config; do $$ARGV[0]; print $$config{"iccm"}{"iccm_sadr"}' "$(EL2_CONFIG_FILE)")
DTCM_START_ADDR = $(shell perl -e 'my $$config; do $$ARGV[0]; print $$config{"dccm"}{"dccm_sadr"}' "$(EL2_CONFIG_FILE)")
ITCM_ELF_SECTIONS = --only-section=.text
DTCM_ELF_SECTIONS = --only-section=.data --only-section=.sdata --only-section=.rodata --only-section=.srodata --only-section=.bss --only-section=.sbss

picolibc:
	$(MAKE) -f ${RV_ROOT}/tools/picolibc.mk all

# TODO: Make .hex file for SRAM with code and data on SRAM
# $(TEST).exe: picolibc $(OBJS) ${SNAPSHOT_DIR}/defines.h
$(TEST).elf: $(OBJS) $(COMMON_OBJS) picolibc ${SNAPSHOT_DIR}/defines.h
	@echo Building $(TEST).elf
	$(GCC_PREFIX)-gcc $(LD_ABI) $(LD_FLAGS) $(OBJS) $(COMMON_OBJS) -o $(TEST).elf
	$(GCC_PREFIX)-objdump -S -t -j .data -j .text $(TEST).elf > $(TEST).dis
	$(GCC_PREFIX)-nm -B -n $(TEST).elf > $(TEST).sym
	$(GCC_PREFIX)-objcopy -O verilog $(TEST).elf $(ITCM_ELF_SECTIONS) --change-addresses "-$(ITCM_START_ADDR)" $(TEST).itcm.hex
	$(GCC_PREFIX)-objcopy -O verilog $(TEST).elf $(DTCM_ELF_SECTIONS) --change-addresses "-$(DTCM_START_ADDR)" $(TEST).dtcm.hex
	@echo Completed building $(TEST)
# $(GCC_PREFIX)-objcopy -O verilog $(TEST).exe $(TEST).hex

%.o : %.s ${SNAPSHOT_DIR}/defines.h
	$(GCC_PREFIX)-cpp $(INCS)  $<  > $*.cpp.s
	$(GCC_PREFIX)-as ${CC_ABI} $*.cpp.s -o $@

%.o : %.c picolibc ${SNAPSHOT_DIR}/defines.h
	$(GCC_PREFIX)-gcc ${INCS} ${C_FLAGS} -DCOMPILER_FLAGS="\"${C_FLAGS}\"" -c $< -o $@

$(SEP_TESTS_COMMON_DIR)/%.o : $(SEP_TESTS_COMMON_DIR)/%.s ${SNAPSHOT_DIR}/defines.h
	$(GCC_PREFIX)-cpp $(INCS)  $<  > $*.cpp.s
	$(GCC_PREFIX)-as ${CC_ABI} $*.cpp.s -o $@

$(SEP_TESTS_COMMON_DIR)/%.o : $(SEP_TESTS_COMMON_DIR)/%.c picolibc ${SNAPSHOT_DIR}/defines.h
	$(GCC_PREFIX)-gcc ${INCS} ${C_FLAGS} -DCOMPILER_FLAGS="\"${C_FLAGS}\"" -c $< -o $@

# TODO: Clean up *.cpp.s files
clean:
	rm -rf $(OBJS) *.dis *.elf *.hex *.map *.su *.sym $(COMMON_OBJS) $(SEP_TESTS_COMMON_DIR)/*.su
