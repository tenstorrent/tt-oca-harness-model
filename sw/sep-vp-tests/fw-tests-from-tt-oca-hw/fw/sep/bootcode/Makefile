SHELL := /usr/bin/env
.SHELLFLAGS := bash -c

# OSEP SEP Boot ROM
#
# Code (.text), metadata, read-only data (.rodata), and .data initial values
# are loaded through BOOT_ROM_VMEM_FILE. Runtime .data/.bss/stack live in DCCM.
#
# Outputs:
#   build/boot_rom.elf      - linked at ROM_BASE (0x10040000)
#   build/boot_rom.vmem     - 64-bit VMEM for Boot ROM preload ($readmemh)
#   build/boot_rom.itcm.hex - Verilog hex for BACKDOOR_TCM ICCM preload (legacy)
#   build/boot_rom.dtcm.hex - empty compatibility file; ROM initializes DCCM
#
# Prereqs:
#   - riscv toolchain in PATH (auto-detected; see GCC_PREFIX below)

# Set GCC_PREFIX in the environment or on the command line to pin a toolchain.
# Otherwise probe the prefixes the supported hosts ship, so a bare `make` works
# everywhere: Ubuntu/RHEL install riscv64-unknown-elf-, macOS Homebrew installs
# riscv64-elf-, and the xpack builds use riscv-none-elf-. The recipes append the
# dash themselves ($(GCC_PREFIX)-gcc), so these candidates are dashless.
# RISCV_TOOLCHAIN_PATH points at a toolchain that is not on PATH (RHEL module
# trees, bundled zips). Mirrors sw/sep-vp-tests/Makefile.common.
GCC_PREFIX_CANDIDATES ?= riscv64-unknown-elf riscv64-elf riscv-none-elf \
                         riscv64-linux-gnu riscv-none-embed

ifneq ($(strip $(RISCV_TOOLCHAIN_PATH)),)
RISCV_BINDIR := $(RISCV_TOOLCHAIN_PATH)/bin/
endif

ifeq ($(origin GCC_PREFIX), undefined)
GCC_PREFIX := $(firstword $(foreach p,$(GCC_PREFIX_CANDIDATES), \
                $(if $(shell command -v $(RISCV_BINDIR)$(p)-gcc 2>/dev/null),$(RISCV_BINDIR)$(p))))
endif

ifeq ($(strip $(GCC_PREFIX)),)
ifeq ($(filter clean,$(MAKECMDGOALS)),)
$(error No RISC-V toolchain found. Set GCC_PREFIX or RISCV_TOOLCHAIN_PATH, or install one: \
macOS "brew install riscv64-elf-gcc"; Ubuntu "sudo apt install gcc-riscv64-unknown-elf"; \
RHEL/TT "module load riscv-gnu-toolchain/2025.01.20-rhel-8.10")
endif
endif

# elf-to-vmem.py needs pyelftools, which is rarely present in a system python.
# dependencies/setup_dependencies.sh already puts it in a venv for the OTBN
# tools, so reach for that interpreter when it exists and fall back otherwise.
BOOTCODE_VENV_PYTHON := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))/../../../dependencies/build/.toolenv/bin/python3
PYTHON3 ?= $(if $(wildcard $(BOOTCODE_VENV_PYTHON)),$(BOOTCODE_VENV_PYTHON),python3)

# Boot ROM base address (SEP_BOOT_ROM_MEM_BASE_ADDR from register map).
ROM_BASE  ?= 0x10040000
DCCM_BASE ?= 0xC0040000

# DCCM scrub bytes before .data/.bss init (0 disables scrub).
# Default: disabled (0) for RTL simulation speed; UVM init_tcms() initializes
# DCCM ECC before reset is released, and ROM code copies/clears runtime data.
DCCM_SCRUB_BYTES ?= 0

# EXT SRAM scrub bytes before manifest load (0 disables scrub).
# Default: disabled (0) because the 256 KiB CPU store loop is extremely slow
# in RTL simulation (~65K AXI writes).  Manifest DMA will overwrite the needed
# region anyway, so skipping the scrub is safe for DV.
SRAM_SCRUB_BYTES ?= 0

# PMP knobs (default keeps current behavior):
# - PMP_ENABLE=1: program PMP CSRs in vector.S (default)
# - PMP_LOCK=0: do not set L bit; PMP_LOCK=1 sets L bit for programmed entries
PMP_ENABLE ?= 1
PMP_LOCK   ?= 0

# SPI boot controller select (build-time). 0 = Cadence xSPI (default, unchanged);
# 1 = OpenTitan SPI host. Exactly one controller is used per ROM image; the
# unselected driver is dropped by --gc-sections.
BOOT_SPI_CONTROLLER_OT ?= 0

# OpenTitan RX-FIFO drain method (build-time; only meaningful when
# BOOT_SPI_CONTROLLER_OT=1). 0 = secure DMA (default), 1 = CPU programmed I/O.
# PIO reads correctly against a fill-then-drain SPI model; DMA offloads the copy
# via the RX-watermark hardware handshake.
BOOT_OT_SPI_USE_PIO ?= 0

# OpenTitan boot profile index (build-time; only meaningful when
# BOOT_SPI_CONTROLLER_OT=1). 0 = safe default parameter set; set to another row of
# ot_spi_profiles[] to boot a different attached flash part.
BOOT_OT_SPI_PROFILE ?= 0

BUILD_TYPE ?= test

ifeq ($(BUILD_TYPE),release)
	CFLAGS_BUILD_TYPE := -DRELEASE_BUILD=1
	DEBUG ?= 0
else ifeq ($(BUILD_TYPE),test)
	CFLAGS_BUILD_TYPE := -DTEST_BUILD=1
	DEBUG ?= 1
endif

BUILD_DIR := build
LINK_DIR  := link
SRC_DIR   := src
TOOLS_DIR := tools
SCRIPTS_DIR := scripts
INCLUDE_DIR := include
PREBUILT_DIR := prebuilt
BL0_VERSION := $(shell $(PYTHON3) $(SCRIPTS_DIR)/bl0-version.py --build-type $(BUILD_TYPE) 2>/dev/null || echo "unknown")
OCH_REGS_INCLUDE_DIR := ../../../meta/registers/c

ELF      := $(BUILD_DIR)/boot_rom.elf
VMEM     := $(BUILD_DIR)/boot_rom.vmem
ITCM_HEX := $(BUILD_DIR)/boot_rom.itcm.hex
DTCM_HEX := $(BUILD_DIR)/boot_rom.dtcm.hex

# SPI flash image for manifest boot testing (bl1_pass_test as BL1).
BL1_TEST_DIR         := ../tests/bl1_pass_test
BL1_TEST_BIN         := $(BL1_TEST_DIR)/build/bl1_pass_test.bin
NON_SECURE_BOOT_SPI  := $(BUILD_DIR)/non_secure_boot.spi_preload

# tt-boot-manifest: external tool for generating signed/encrypted SPI images.
# Install: git submodule add git@github.com:tenstorrent/tt-boot-manifest.git fw/deps/tt-boot-manifest
#          pip install -e fw/deps/tt-boot-manifest
TT_BOOT_MANIFEST_DIR  := ../../deps/tt-boot-manifest
CONFIGS_DIR           := configs
SECURE_BOOT_SPI       := $(BUILD_DIR)/secure_boot.spi_preload

INSERT_SHA256 := $(TOOLS_DIR)/insert-rom-sha256.py
ELF_TO_VMEM   := $(TOOLS_DIR)/elf-to-vmem.py

OBJS := $(BUILD_DIR)/vector.o $(BUILD_DIR)/rom_main.o $(BUILD_DIR)/rom_metadata.o \
	$(BUILD_DIR)/boot_straps.o $(BUILD_DIR)/pll_init.o \
	$(BUILD_DIR)/sep_spi.o $(BUILD_DIR)/sep_ot_spi.o $(BUILD_DIR)/sep_dma.o \
	$(BUILD_DIR)/manifest_load.o $(BUILD_DIR)/rom_handoff.o \
	$(BUILD_DIR)/rom_mem_clear.o $(BUILD_DIR)/status_ring.o \
	$(BUILD_DIR)/hmac_sha256.o \
	$(BUILD_DIR)/fuse_lock.o \
	$(BUILD_DIR)/lifecycle.o \
	$(BUILD_DIR)/manifest_crypto.o \
	$(BUILD_DIR)/otbn_driver.o \
	$(BUILD_DIR)/rsa_verify.o \
	$(BUILD_DIR)/aes_driver.o \
	$(BUILD_DIR)/kdf.o \
	$(BUILD_DIR)/key_digests.o \
	$(BUILD_DIR)/otbn/rsa_3072_app_otbn.o

# Be conservative: build without RVC (compressed) and disable relaxation, so the
# resulting ROM image runs even if the core is configured as rv32im (no 'C').
CC_ABI   := -march=rv32im -mabi=ilp32

# Zicsr / Zifencei rescue, the same probe fw/sep/tests/common/common.mk uses.
# Toolchains following ISA manual 20191213 or later split the CSR instructions
# out of the "I" base into Zicsr, so vector.S stops assembling under a plain
# rv32im.  The TT toolchain predates the split and still accepts them, so probe
# the assembler instead of changing the string for everyone.
CC_MARCH := $(patsubst -march=%,%,$(firstword $(filter -march=%,$(CC_ABI))))
ifneq ($(findstring zicsr,$(CC_MARCH)),zicsr)
CSR_ACCEPTED := $(shell echo 'csrw 0x7c0,a5' | \
	$(GCC_PREFIX)-as -march=$(CC_MARCH) -mabi=ilp32 -o /dev/null - 2>/dev/null && echo yes)
ifneq ($(CSR_ACCEPTED),yes)
CC_ABI := -march=$(CC_MARCH)_zicsr_zifencei -mabi=ilp32
endif
endif
# Pin the C standard explicitly rather than relying on the compiler default, so the
# build is reproducible and portable. TODO: also promote warnings to errors
# (-Wall -Wextra -Werror) once a clean build is confirmed.
CFLAGS   := -std=c11 -Os -ffreestanding -fno-builtin -fdata-sections -ffunction-sections -mno-relax \
	-I$(INCLUDE_DIR) \
	-I$(OCH_REGS_INCLUDE_DIR) \
	-I$(BUILD_DIR)/otbn \
	-DDCCM_SCRUB_BYTES=$(DCCM_SCRUB_BYTES) \
	-DSRAM_SCRUB_BYTES=$(SRAM_SCRUB_BYTES) \
	-DPMP_ENABLE=$(PMP_ENABLE) -DPMP_LOCK=$(PMP_LOCK) \
	-DBOOT_SPI_CONTROLLER_OT=$(BOOT_SPI_CONTROLLER_OT) \
	-DBOOT_OT_SPI_USE_PIO=$(BOOT_OT_SPI_USE_PIO) \
	-DBOOT_OT_SPI_PROFILE=$(BOOT_OT_SPI_PROFILE) \
	$(CFLAGS_BUILD_TYPE) \
	-DSEP_BL0 \
	$(CC_ABI)
ASFLAGS  := -x assembler-with-cpp -mno-relax \
	-I$(INCLUDE_DIR) \
	-DDCCM_SCRUB_BYTES=$(DCCM_SCRUB_BYTES) \
	-DPMP_ENABLE=$(PMP_ENABLE) -DPMP_LOCK=$(PMP_LOCK) \
	$(CC_ABI)
LDFLAGS  := -nostdlib -Wl,--gc-sections -Wl,--no-relax -T $(LINK_DIR)/rom.ld $(CC_ABI)

ifeq ($(DEBUG),1)
	CFLAGS += -DDEBUG
	ASFLAGS += -DDEBUG
endif

# --- Rebuild on build-flag changes ---
# Objects are keyed on source mtime only, but their content also depends on the
# build-time -D flags injected into CFLAGS/ASFLAGS. Without this, `make
# BOOT_OT_SPI_USE_PIO=1` (or a profile / build-type change) with unchanged sources
# would silently reuse stale objects and produce a mislabeled image. Record the
# codegen-affecting flags in a stamp file, rewritten only when they change, and
# make every object depend on it so a flag change forces a full rebuild.
BUILD_FLAGS := CTRL_OT=$(BOOT_SPI_CONTROLLER_OT) USE_PIO=$(BOOT_OT_SPI_USE_PIO) \
	PROFILE=$(BOOT_OT_SPI_PROFILE) BUILD_TYPE=$(BUILD_TYPE) DEBUG=$(DEBUG) \
	PMP_ENABLE=$(PMP_ENABLE) PMP_LOCK=$(PMP_LOCK) \
	DCCM_SCRUB=$(DCCM_SCRUB_BYTES) SRAM_SCRUB=$(SRAM_SCRUB_BYTES)
FLAGS_STAMP := $(BUILD_DIR)/.build_flags

# FORCE the recipe to run every build; cmp-and-move only bumps the stamp's mtime
# when the flags actually change, so unchanged builds do not rebuild.
$(FLAGS_STAMP): FORCE | $(BUILD_DIR)
	@echo '$(BUILD_FLAGS)' > "$@.tmp"
	@if ! cmp -s "$@.tmp" "$@" 2>/dev/null; then \
		echo "  [build flags changed -> rebuilding objects]"; \
		mv "$@.tmp" "$@"; \
	else \
		rm "$@.tmp"; \
	fi

$(OBJS): $(FLAGS_STAMP)

.PHONY: FORCE
FORCE:

# Sections that go into Boot ROM for legacy ICCM preload generation.
ROM_ELF_SECTIONS := --only-section=.text --only-section=.text.* --only-section=.metadata --only-section=.metadata.* \
	--only-section=.rodata --only-section=.rodata.* --only-section=.srodata --only-section=.srodata.*

.PHONY: all clean non_secure_boot_spi secure_boot_spi

all: $(ELF) $(VMEM) $(ITCM_HEX) $(DTCM_HEX)

# Build the non-secure SPI flash image with hello_world BL1 payload.
# Separate target so it doesn't block the default ROM build.
non_secure_boot_spi: $(NON_SECURE_BOOT_SPI)

# Build the secure SPI flash image used by the secure-boot test.
secure_boot_spi: $(SECURE_BOOT_SPI)

$(BUILD_DIR):
	mkdir -p "$(BUILD_DIR)"

$(BUILD_DIR)/vector.o: $(SRC_DIR)/vector.S | $(BUILD_DIR)
	$(GCC_PREFIX)-gcc $(CFLAGS) $(ASFLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/rom_main.o: $(SRC_DIR)/rom_main.c | $(BUILD_DIR)
	$(GCC_PREFIX)-gcc $(CFLAGS) -c "$<" -o "$@"

# Generate the version header from git describe.
$(BUILD_DIR)/bl0_version.h: $(SCRIPTS_DIR)/bl0-version.py | $(BUILD_DIR)
	@echo '#define BL0_VERSION "$(BL0_VERSION)"' > "$@.tmp"
	@if ! cmp -s "$@.tmp" "$@" 2>/dev/null; then mv "$@.tmp" "$@"; else rm "$@.tmp"; fi

$(BUILD_DIR)/rom_metadata.o: $(SRC_DIR)/rom_metadata.c $(BUILD_DIR)/bl0_version.h | $(BUILD_DIR)
	$(GCC_PREFIX)-gcc $(CFLAGS) -I$(BUILD_DIR) -c "$<" -o "$@"

$(BUILD_DIR)/boot_straps.o: $(SRC_DIR)/boot_straps.c | $(BUILD_DIR)
	$(GCC_PREFIX)-gcc $(CFLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/pll_init.o: $(SRC_DIR)/pll_init.c | $(BUILD_DIR)
	$(GCC_PREFIX)-gcc $(CFLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/sep_spi.o: $(SRC_DIR)/sep_spi.c | $(BUILD_DIR)
	$(GCC_PREFIX)-gcc $(CFLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/sep_ot_spi.o: $(SRC_DIR)/sep_ot_spi.c | $(BUILD_DIR)
	$(GCC_PREFIX)-gcc $(CFLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/sep_dma.o: $(SRC_DIR)/sep_dma.c | $(BUILD_DIR)
	$(GCC_PREFIX)-gcc $(CFLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/manifest_load.o: $(SRC_DIR)/manifest_load.c | $(BUILD_DIR)
	$(GCC_PREFIX)-gcc $(CFLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/rom_handoff.o: $(SRC_DIR)/rom_handoff.c | $(BUILD_DIR)
	$(GCC_PREFIX)-gcc $(CFLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/rom_mem_clear.o: $(SRC_DIR)/rom_mem_clear.c | $(BUILD_DIR)
	$(GCC_PREFIX)-gcc $(CFLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/status_ring.o: $(SRC_DIR)/status_ring.c | $(BUILD_DIR)
	$(GCC_PREFIX)-gcc $(CFLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/hmac_sha256.o: $(SRC_DIR)/hmac_sha256.c | $(BUILD_DIR)
	$(GCC_PREFIX)-gcc $(CFLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/fuse_lock.o: $(SRC_DIR)/fuse_lock.c | $(BUILD_DIR)
	$(GCC_PREFIX)-gcc $(CFLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/lifecycle.o: $(SRC_DIR)/lifecycle.c | $(BUILD_DIR)
	$(GCC_PREFIX)-gcc $(CFLAGS) -c "$<" -o "$@"

# --- C13.10 secure boot crypto objects ---

# OTBN RSA-3072 application — built using the common_otbn infrastructure.
# Assembles the OTBN .s sources, links, and generates a C header with
# IMEM/DMEM arrays that get compiled into Boot ROM .rodata.
OCH_ROOT_ABS := $(shell cd ../../.. && pwd)
OTBN_APP_NAME := rsa_3072_app
OTBN_SRC_DIR  := ../tests/otbn_rsa_3072_verify_test/otbn_src
OTBN_BUILD_DIR := $(BUILD_DIR)/otbn
OTBN_APP_SRCS := $(OTBN_SRC_DIR)/gcd.s $(OTBN_SRC_DIR)/modexp.s \
	$(OTBN_SRC_DIR)/montmul.s $(OTBN_SRC_DIR)/mul.s \
	$(OTBN_SRC_DIR)/rsa_keygen.s $(OTBN_SRC_DIR)/rsa_modinv_f4.s \
	$(OTBN_SRC_DIR)/rsa_primality.s $(OTBN_SRC_DIR)/run_rsa_mem.s \
	$(OTBN_SRC_DIR)/run_rsa.s

# Set OCH_ROOT before including otbn_app.mk (it checks for this).
OCH_ROOT := $(OCH_ROOT_ABS)
include $(OCH_ROOT_ABS)/fw/sep/tests/common_otbn/otbn_app.mk

# Aliases for generated OTBN files.
OTBN_APP_H := $(OTBN_APP_H_FILE)
OTBN_APP_C := $(OTBN_APP_C_FILE)

# Compile generated OTBN C file (contains IMEM/DMEM arrays).
# Uses relaxed flags: no stack-usage analysis (OTBN data, not real code).
$(BUILD_DIR)/otbn/rsa_3072_app_otbn.o: $(OTBN_APP_C) $(OTBN_APP_H)
	$(GCC_PREFIX)-gcc $(CFLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/manifest_crypto.o: $(SRC_DIR)/manifest_crypto.c | $(BUILD_DIR)
	$(GCC_PREFIX)-gcc $(CFLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/otbn_driver.o: $(SRC_DIR)/otbn_driver.c $(OTBN_APP_H) | $(BUILD_DIR)
	$(GCC_PREFIX)-gcc $(CFLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/rsa_verify.o: $(SRC_DIR)/rsa_verify.c $(OTBN_APP_H) | $(BUILD_DIR)
	$(GCC_PREFIX)-gcc $(CFLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/aes_driver.o: $(SRC_DIR)/aes_driver.c | $(BUILD_DIR)
	$(GCC_PREFIX)-gcc $(CFLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/kdf.o: $(SRC_DIR)/kdf.c | $(BUILD_DIR)
	$(GCC_PREFIX)-gcc $(CFLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/key_digests.o: $(SRC_DIR)/key_digests.c | $(BUILD_DIR)
	$(GCC_PREFIX)-gcc $(CFLAGS) -c "$<" -o "$@"

$(ELF): $(OBJS) $(LINK_DIR)/rom.ld $(INSERT_SHA256)
	$(GCC_PREFIX)-gcc $(LDFLAGS) $(OBJS) -o "$@"
	$(PYTHON3) $(INSERT_SHA256) --gcc-prefix $(GCC_PREFIX) -o "$@" "$@"
	$(GCC_PREFIX)-objdump -d -S "$@" > "$(BUILD_DIR)/boot_rom.dis"
	$(GCC_PREFIX)-nm -B -n "$@" > "$(BUILD_DIR)/boot_rom.sym"

# 64-bit VMEM for Boot ROM preload (used by +BOOT_ROM_VMEM_FILE in DV).
$(VMEM): $(ELF) $(ELF_TO_VMEM)
	$(PYTHON3) $(ELF_TO_VMEM) --base $(ROM_BASE) -o "$@" "$<"

# Verilog hex for BACKDOOR_TCM ICCM preload (legacy — code is in ROM now,
# but BACKDOOR_TCM still loads this into ICCM; content is harmless).
$(ITCM_HEX): $(ELF)
	$(GCC_PREFIX)-objcopy -O verilog "$(ELF)" $(ROM_ELF_SECTIONS) \
		--change-addresses "-$(ROM_BASE)" "$(ITCM_HEX)"

# Empty compatibility file. Boot ROM tests should use DATA_SIZE=0 so UVM skips
# DCCM preload; vector.S copies .data from ROM and clears .bss after reset.
$(DTCM_HEX): $(ELF)
	: > "$(DTCM_HEX)"

# --- SPI flash image for manifest boot testing ---

# Always delegate to sub-make so it can check its own dependencies
# (bl1_pass_test.c → .o → .elf → .bin).
.PHONY: bl1_test_build
bl1_test_build:
	$(MAKE) -C "$(BL1_TEST_DIR)" GCC_PREFIX=$(GCC_PREFIX)

# --- SPI images via tt-boot-manifest ---
# All SPI images are now generated from YAML configs.
# tt-boot-manifest outputs raw binary → bin_to_spi_preload.py converts to Verilog hex.

# Ensure Python dependencies are installed (tt-boot-manifest, cryptography).
.PHONY: ensure-python-deps
ensure-python-deps:
	@$(PYTHON3) -c "import tt_boot_manifest" 2>/dev/null || \
		(echo "Installing Python dependencies from requirements.txt..." && \
		 OCH_ROOT=$(OCH_ROOT_ABS) pip3 install -r requirements.txt 2>/dev/null || \
		 OCH_ROOT=$(OCH_ROOT_ABS) pip3 install --break-system-packages -r requirements.txt)

# Helper: build an SPI image from a YAML config.
# Usage: $(call build_spi,<config_yaml>,<output_spi_preload>)
define build_spi
	ROOT=$(TT_BOOT_MANIFEST_DIR) OCH_ROOT=$(OCH_ROOT_ABS) \
		$(PYTHON3) -m tt_boot_manifest.pack_images \
		--config "$(1)" --out "$(2).bin" -v
	$(PYTHON3) $(TOOLS_DIR)/bin_to_spi_preload.py "$(2).bin" "$(2)"
endef

# Non-secure boot SPI image.
# Used by sep_rom_non_secure_boot_test.
$(NON_SECURE_BOOT_SPI): bl1_test_build ensure-python-deps | $(BUILD_DIR)
	$(call build_spi,$(CONFIGS_DIR)/non_secure_boot_test.yaml,$(NON_SECURE_BOOT_SPI))

# Secure boot SPI image.
# Used by sep_rom_secure_boot_test.
$(SECURE_BOOT_SPI): bl1_test_build ensure-python-deps | $(BUILD_DIR)
	$(call build_spi,$(CONFIGS_DIR)/secure_boot_test.yaml,$(SECURE_BOOT_SPI))

clean:
	rm -rf "$(BUILD_DIR)"
