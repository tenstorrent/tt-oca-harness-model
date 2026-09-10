# TODO: Error out if RV_ROOT is not set
# TODO: Error out if TEST is not set

.DEFAULT_GOAL := all

SEP_TESTS_COMMON_DIR = $(SEP_TESTS_DIR)/common

SEP_TOP_REG_DIR = $(OCH_ROOT)/meta/registers/c

MAKE ?= make
GCC_PREFIX ?= riscv64-unknown-elf

PICOLIBC_DIR ?= ${RV_ROOT}/third_party/picolibc/install

SNAPSHOT ?= sep
SNAPSHOT_DIR = ${RV_ROOT}/snapshots/${SNAPSHOT}

# NOTE:
# This common.mk is used to build firmware tests under fw/sep/tests.
# Keep it self-contained and do not depend on dv/sep.
SEP_TESTS_DIR = ${OCH_ROOT}/fw/sep/tests

ABI ?= -mabi=ilp32
# xPack GCC 15+ has no rv32imac multilib; rv32imc is the closest match and
# keeps picolibc.specs %M pointed at a directory that was actually built.
ifeq ($(shell $(GCC_PREFIX)-gcc -print-multi-lib 2>/dev/null | grep -c '^rv32imac/'),0)
LD_ABI ?= $(ABI) -march=rv32imc
else
LD_ABI ?= $(ABI) -march=rv32imac
endif
TEST_LIBS ?=

# VP and silicon now share the same memory map (ICCM 0xC0000000,
# DCCM 0xC0040000, peripherals 0x10xxxxxx, PIC 0xC0080000), so a single
# crt0.s and linker script serve both. PLATFORM is retained as a knob in
# case a future RTL flow needs to override LINKER_SCRIPT externally.
PLATFORM ?= vp
LINKER_SCRIPT ?= $(SEP_TESTS_DIR)/common/exec_from_tcms.ld

# Override march depending on used GCC version
ifneq ($(shell which $(GCC_PREFIX)-gcc 2> /dev/null),)
	GCCVERSIONGT11 := $(shell expr `$(GCC_PREFIX)-gcc -dumpversion | cut -f1 -d.` \>= 11)
	ifeq "$(GCCVERSIONGT11)" "1"
		CC_ABI = $(ABI) -march=rv32imc_zicsr_zifencei_zba_zbb_zbc
	else
		CC_ABI = $(ABI) -march=rv32imc
	endif
endif

EXT_INCS ?= -I$(OCH_ROOT)/hw/comp/uart_16550/data/registers/c # TODO: Move to appropriate location
# Always include the common headers directory, even if a testcase pre-sets EXT_INCS.
EXT_INCS += -I$(SEP_TESTS_DIR)/common -I$(SEP_TOP_REG_DIR)

INCS ?= -I$(SNAPSHOT_DIR) -isystem $(PICOLIBC_DIR)/picolibc/$(GCC_PREFIX)/include $(EXT_INCS)
LIBS ?= $(EXT_LIBS)

C_FLAGS = -Os -fdata-sections -ffunction-sections -fno-common -fstack-usage $(INCS) $(CC_ABI) $(EXT_C_FLAGS)

# Zicsr / Zifencei rescue.
#
# Toolchains implementing ISA-manual 20191213+ split csrr/csrw out of the "I"
# base into Zicsr, so an -march string written before the split (several tests
# force plain -march=rv32imc through EXT_C_FLAGS to keep the Zb* extensions out)
# now fails to assemble the CSR accesses in those tests and in crt0.s with
# "unrecognized opcode ... extension `zicsr' required".  The TT toolchain
# predates the split and accepts them, so the strings cannot simply be changed.
# Probe the effective -march instead and re-add the extensions when the
# assembler needs them; the appended flag wins because GCC takes the last one.
EFFECTIVE_MARCH := $(patsubst -march=%,%,$(lastword $(filter -march=%,$(C_FLAGS))))
ifneq ($(EFFECTIVE_MARCH),)
ifneq ($(findstring zicsr,$(EFFECTIVE_MARCH)),zicsr)
CSR_ACCEPTED := $(shell echo 'csrw 0x7c0,a5' | \
    $(GCC_PREFIX)-as -march=$(EFFECTIVE_MARCH) $(ABI) -o /dev/null - 2>/dev/null && echo yes)
ifneq ($(CSR_ACCEPTED),yes)
C_FLAGS += -march=$(EFFECTIVE_MARCH)_zicsr_zifencei
CC_ABI += -march=$(EFFECTIVE_MARCH)_zicsr_zifencei
endif
endif
endif
LD_FLAGS = -Wl,--gc-sections -Wl,-Map=$(TEST).map -T$(LINKER_SCRIPT) --specs=$(PICOLIBC_DIR)/picolibc.specs $(LIBS) -nostartfiles $(EXT_LD_FLAGS)

COMMON_C_SRCS = $(SEP_TESTS_COMMON_DIR)/init_stdout.c
COMMON_ASM_SRCS = $(SEP_TESTS_COMMON_DIR)/crt0.s
COMMON_OBJS = $(COMMON_C_SRCS:.c=.o) $(COMMON_ASM_SRCS:.s=.o)

C_SRCS ?= $(wildcard *.c)
ASM_SRCS ?= $(filter-out %.cpp.s,$(wildcard *.s))
OBJS = $(C_SRCS:.c=.o) $(ASM_SRCS:.s=.o)

# ITCM/DTCM base addresses for hex file extraction (--change-addresses makes hex relative to 0).
# VP: use fixed VP addresses. RTL: read from EL2 snapshot perl_configs.pl.
ifeq ($(PLATFORM),vp)
    ITCM_START_ADDR = 0x01000000
    DTCM_START_ADDR = 0x00000000
else
    EL2_CONFIG_FILE = $(SNAPSHOT_DIR)/perl_configs.pl
    ITCM_START_ADDR = $(shell perl -e 'my $$config; do $$ARGV[0]; print $$config{"iccm"}{"iccm_sadr"}' "$(EL2_CONFIG_FILE)")
    DTCM_START_ADDR = $(shell perl -e 'my $$config; do $$ARGV[0]; print $$config{"dccm"}{"dccm_sadr"}' "$(EL2_CONFIG_FILE)")
endif
ITCM_ELF_SECTIONS = --only-section=.text --only-section=.nmi_handler
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

clean:
	rm -rf $(OBJS) *.dis *.elf *.hex *.map *.su *.sym $(COMMON_OBJS) $(SEP_TESTS_COMMON_DIR)/*.su *.cpp.s $(SEP_TESTS_COMMON_DIR)/*.cpp.s
