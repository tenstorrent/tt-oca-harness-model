# OTBN Application Build Infrastructure
# Generic Makefile for building OTBN applications and generating C arrays
#
# This replaces custom Python build scripts with a reusable Makefile-based flow
# that leverages vendored OpenTitan infrastructure in vendor/opentitan/hw/ip/otbn/
#
# Usage in test Makefile:
#   OTBN_APP_NAME = my_app
#   OTBN_APP_SRCS = src/file1.s src/file2.s
#   include $(OCH_ROOT)/dv/sep/tests/common_otbn/otbn_app.mk
#
# Outputs:
#   $(OTBN_APP_NAME)_otbn.c - Single C file with memory arrays and CRC values
#   $(OTBN_APP_NAME)_otbn.h - Header with extern declarations

ifndef OCH_ROOT
$(error OCH_ROOT is not set. Please run: export OCH_ROOT=<your_och_path>)
endif

# Directory structure
COMMON_OTBN_DIR := $(OCH_ROOT)/dv/sep/tests/common_otbn
OCH_OTBN_DIR    := $(OCH_ROOT)/vendor/opentitan/hw/ip/otbn
OTBN_UTIL_DIR   := $(OCH_OTBN_DIR)/util
OTBN_DATA_DIR   := $(OCH_OTBN_DIR)/data

# Build tools from vendored OpenTitan OTBN
#
# otbn_as.py imports PyYAML.  dependencies/setup_dependencies.sh installs it
# into a venv beside this tree so nothing has to be added to the system python;
# fall back to plain python3 when that venv is not there.
OTBN_VENV_PYTHON := $(OCH_ROOT)/dependencies/build/.toolenv/bin/python3
PYTHON3 ?= $(if $(wildcard $(OTBN_VENV_PYTHON)),$(OTBN_VENV_PYTHON),python3)

OTBN_AS := $(PYTHON3) $(OTBN_UTIL_DIR)/otbn_as.py
OTBN_OBJDUMP := $(PYTHON3) $(OTBN_UTIL_DIR)/otbn_objdump.py

# The OTBN application is linked as rv32 with whichever bare-metal binutils are
# installed; only the TT machines have the riscv32-unknown-elf spelling.
RV32_PREFIX ?= $(if $(shell command -v riscv32-unknown-elf-ld 2>/dev/null),riscv32-unknown-elf,$(GCC_PREFIX))
RV32_LD = $(RV32_PREFIX)-ld
# A riscv64 ld defaults to elf64 emulation and refuses the rv32 OTBN objects.
RV32_LD_EMUL ?= -m elf32lriscv
RV32_OBJCOPY = $(RV32_PREFIX)-objcopy
RV32_OBJDUMP = $(RV32_PREFIX)-objdump

# otbn_as.py resolves binutils itself (shared/toolchain.py) and only knows the
# riscv32-unknown-elf spelling unless RV32_TOOL_<TOOL> holds an absolute path.
otbn_tool_path = $(shell command -v $(RV32_PREFIX)-$(1) 2>/dev/null)
otbn_tool_env  = $(if $(call otbn_tool_path,$(2)),RV32_TOOL_$(1)="$(call otbn_tool_path,$(2))")
OTBN_TOOL_ENV := $(call otbn_tool_env,AS,as) $(call otbn_tool_env,LD,ld) \
                 $(call otbn_tool_env,OBJDUMP,objdump) $(call otbn_tool_env,OBJCOPY,objcopy) \
                 $(call otbn_tool_env,AR,ar) $(call otbn_tool_env,NM,nm) \
                 $(call otbn_tool_env,STRIP,strip)

# Default values (can be overridden by including Makefile)
OTBN_APP_NAME ?= otbn_app
OTBN_SRC_DIR ?= otbn_src
OTBN_APP_SRCS ?= $(wildcard $(OTBN_SRC_DIR)/*.s)
OTBN_BUILD_DIR ?= otbn_build

# Check required variables
ifndef OTBN_APP_NAME
$(error OTBN_APP_NAME must be defined by including Makefile)
endif

ifeq ($(OTBN_APP_SRCS),)
$(error OTBN_APP_SRCS must be defined - no .s files found)
endif

# Output files (placed in build directory to keep source directory clean)
OTBN_APP_C_FILE := $(OTBN_BUILD_DIR)/$(OTBN_APP_NAME)_otbn.c
OTBN_APP_H_FILE := $(OTBN_BUILD_DIR)/$(OTBN_APP_NAME)_otbn.h

# Intermediate files
OTBN_APP_OBJS := $(patsubst %.s,$(OTBN_BUILD_DIR)/%.o,$(notdir $(OTBN_APP_SRCS)))
OTBN_APP_ELF := $(OTBN_BUILD_DIR)/$(OTBN_APP_NAME).elf
OTBN_APP_DISASM := $(OTBN_BUILD_DIR)/$(OTBN_APP_NAME).dis
OTBN_APP_SYMS := $(OTBN_BUILD_DIR)/$(OTBN_APP_NAME).sym
OTBN_APP_IMEM_BIN := $(OTBN_BUILD_DIR)/imem.bin
OTBN_APP_DMEM_BIN := $(OTBN_BUILD_DIR)/dmem.bin
OTBN_LINKER_SCRIPT := $(COMMON_OTBN_DIR)/otbn_app.ld

# Python environment for OTBN tools
OTBN_PYTHON_ENV := PYTHONPATH="$(OTBN_UTIL_DIR):$(OCH_OTBN_DIR):$(COMMON_OTBN_DIR):$$PYTHONPATH" $(OTBN_TOOL_ENV)

# Main targets
.PHONY: otbn-app otbn-app-clean

otbn-app: $(OTBN_APP_C_FILE) $(OTBN_APP_H_FILE) $(OTBN_APP_DISASM) $(OTBN_APP_SYMS)

# Create build directory
$(OTBN_BUILD_DIR):
	mkdir -p $(OTBN_BUILD_DIR)

# Assembly step: .s -> .o using OTBN assembler
# Primary rule for sources in configurable source directory
$(OTBN_BUILD_DIR)/%.o: $(OTBN_SRC_DIR)/%.s | $(OTBN_BUILD_DIR)
	@echo "Assembling OTBN: $<"
	$(OTBN_PYTHON_ENV) $(OTBN_AS) -o $@ $<


# Alternative assembly rule for sources not in standard directories
$(OTBN_BUILD_DIR)/%.o: %.s | $(OTBN_BUILD_DIR)
	@echo "Assembling OTBN: $<"
	$(OTBN_PYTHON_ENV) $(OTBN_AS) -o $@ $<

# Linking step: .o -> .elf using static linker script
$(OTBN_APP_ELF): $(OTBN_APP_OBJS) $(OTBN_LINKER_SCRIPT) | $(OTBN_BUILD_DIR)
	@echo "Linking OTBN application: $(OTBN_APP_NAME)"
	$(RV32_LD) $(RV32_LD_EMUL) --no-check-sections --no-warn-rwx-segments \
		-T $(OTBN_LINKER_SCRIPT) -o $@ $(OTBN_APP_OBJS)

# Disassembly generation: .elf -> .dis (with all sections and source intermixing)
$(OTBN_APP_DISASM): $(OTBN_APP_ELF)
	@echo "Generating OTBN disassembly"
	$(OTBN_PYTHON_ENV) $(OTBN_OBJDUMP) -D -S -x -h $< > $@

# Symbol table generation: .elf -> .sym
$(OTBN_APP_SYMS): $(OTBN_APP_ELF)
	@echo "Generating OTBN symbol table"
	$(RV32_OBJDUMP) -t $< > $@

# Binary extraction: .elf -> .bin files
$(OTBN_APP_IMEM_BIN): $(OTBN_APP_ELF)
	@echo "Extracting IMEM binary"
	$(RV32_OBJCOPY) -O binary --only-section=.text $< $@

$(OTBN_APP_DMEM_BIN): $(OTBN_APP_ELF)
	@echo "Extracting DMEM binary"
	$(RV32_OBJCOPY) -O binary --only-section=.data $< $@ || touch $@

# C file generation: .bin + .elf -> .c/.h with CRC calculation and symbol extraction
$(OTBN_APP_C_FILE) $(OTBN_APP_H_FILE): $(OTBN_APP_IMEM_BIN) $(OTBN_APP_DMEM_BIN) $(OTBN_APP_ELF)
	@echo "Generating C arrays, CRC values, and symbol addresses for $(OTBN_APP_NAME)"
	$(OTBN_PYTHON_ENV) $(PYTHON3) $(COMMON_OTBN_DIR)/generate_otbn_c.py \
		--app-name $(OTBN_APP_NAME) \
		--imem-bin $(OTBN_APP_IMEM_BIN) \
		--dmem-bin $(OTBN_APP_DMEM_BIN) \
		--output-c $(OTBN_APP_C_FILE) \
		--output-h $(OTBN_APP_H_FILE) \
		--elf-file $(OTBN_APP_ELF)

# Clean targets
otbn-app-clean:
	rm -rf $(OTBN_BUILD_DIR)

# Help target
.PHONY: otbn-app-help
otbn-app-help:
	@echo "OTBN Application Build Targets:"
	@echo "  otbn-app       - Build OTBN application, generate C files, disassembly, and symbols"
	@echo "  otbn-app-clean - Clean OTBN build artifacts"
	@echo ""
	@echo "Required Variables:"
	@echo "  OTBN_APP_NAME - Name of the OTBN application"
	@echo "  OTBN_APP_SRCS - List of .s source files"
	@echo ""
	@echo "Optional Variables:"
	@echo "  OTBN_SRC_DIR   - Source directory for OTBN assembly files (default: otbn_src)"
	@echo "  OTBN_BUILD_DIR - Build directory (default: otbn_build, generates C/H files here too)"
	@echo ""
	@echo "Generated Files (in build directory):"
	@echo "  $(OTBN_APP_NAME)_otbn.c - C array with IMEM/DMEM data"
	@echo "  $(OTBN_APP_NAME)_otbn.h - Header with symbols and declarations"
	@echo ""
	@echo "Include Path:"
	@echo "  Add -I$(OTBN_BUILD_DIR) to your C_FLAGS to include generated headers"
