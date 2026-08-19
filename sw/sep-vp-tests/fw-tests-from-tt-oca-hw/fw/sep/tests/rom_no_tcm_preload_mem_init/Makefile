TEST = rom_no_tcm_preload_mem_init
export TEST

GCC_PREFIX ?= riscv64-unknown-elf
ROM_BASE ?= 0x10040000
OCH_ROOT ?= $(shell git rev-parse --show-toplevel)

ELF = $(TEST).elf
VMEM = $(TEST).vmem

# elf-to-vmem.py imports pyelftools. Same arrangement as common_otbn/otbn_app.mk:
# dependencies/setup_dependencies.sh provisions a venv beside this tree so the
# system python stays untouched; fall back to plain python3 when it is absent.
VMEM_VENV_PYTHON := $(OCH_ROOT)/dependencies/build/.toolenv/bin/python3
PYTHON3 ?= $(if $(wildcard $(VMEM_VENV_PYTHON)),$(VMEM_VENV_PYTHON),python3)

CC_ABI = -march=rv32im_zicsr_zifencei -mabi=ilp32
ASFLAGS = $(CC_ABI) -mno-relax -I$(OCH_ROOT)/fw/sep/tests/common
LDFLAGS = -nostdlib -Wl,--gc-sections -Wl,--no-relax -T rom_only.ld $(CC_ABI)

.PHONY: all clean

all: $(ELF) $(VMEM)

$(TEST).o: $(TEST).S
	$(GCC_PREFIX)-gcc $(ASFLAGS) -c $< -o $@

$(ELF): $(TEST).o rom_only.ld
	$(GCC_PREFIX)-gcc $(LDFLAGS) $(TEST).o -o $@
	$(GCC_PREFIX)-objdump -d -S $@ > $(TEST).dis
	$(GCC_PREFIX)-nm -B -n $@ > $(TEST).sym

$(VMEM): $(ELF)
	$(PYTHON3) $(OCH_ROOT)/fw/sep/bootcode/tools/elf-to-vmem.py \
		--base $(ROM_BASE) --gcc-prefix $(GCC_PREFIX) -o $@ $<

clean:
	rm -f $(TEST).o $(ELF) $(VMEM) $(TEST).dis $(TEST).sym
