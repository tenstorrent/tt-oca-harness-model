#!/usr/bin/env bash
#
# Standalone script for compiling SEP firmware tests
# Does not rely on internal network/modules - external partners can use this
#
# Usage:
#   ./sep_fw_standalone.sh setup          - One-time setup (perl config + picolibc)
#   ./sep_fw_standalone.sh config         - Run EL2 perl configuration only
#   ./sep_fw_standalone.sh picolibc       - Build picolibc only
#   ./sep_fw_standalone.sh build <test>   - Build a specific test (e.g., hello_world)
#   ./sep_fw_standalone.sh build-all      - Build all tests
#   ./sep_fw_standalone.sh clean <test>   - Clean a specific test
#   ./sep_fw_standalone.sh clean-all      - Clean all tests and build artifacts

set -e  # Exit on error

# ============================================================================
# Configuration - External partners should modify these paths
# ============================================================================

# Get the repository root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export OCH_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
export RV_ROOT="$OCH_ROOT/deps/el2"

# RISC-V toolchain configuration
# External partners: Set this to your RISC-V toolchain installation
export GCC_PREFIX="${GCC_PREFIX:-riscv64-unknown-elf}"

# RISC-V Toolchain Path Configuration
# Option 1: If toolchain is already in PATH, nothing needs to be done
# Option 2: Set RISCV_TOOLCHAIN_PATH to your toolchain installation directory
# Option 3: Use environment modules (module load riscv-gnu-toolchain/...)
# Option 4: Use local toolchain in repo's toolchain/ directory (default fallback)
#
# Example configurations:
# export RISCV_TOOLCHAIN_PATH=/opt/riscv
# export RISCV_TOOLCHAIN_PATH=/tools/riscv-gnu-toolchain/2025.01.20-rhel-8.10

# Check if local toolchain exists in repo (for standalone usage)
# Auto-discover toolchain directory from toolchain/ folder
if [ -z "${RISCV_TOOLCHAIN_PATH:-}" ] && [ -d "$OCH_ROOT/toolchain" ]; then
    # Find first directory in toolchain/ that contains the GCC binary
    # Check both direct subdirectories and nested paths (from zip extraction)
    for toolchain_dir in "$OCH_ROOT/toolchain"/*; do
        if [ -d "$toolchain_dir" ]; then
            # Check if it contains the gcc binary (with or without bin/ subdirectory)
            if [ -f "$toolchain_dir/${GCC_PREFIX}-gcc" ] || [ -f "$toolchain_dir/bin/${GCC_PREFIX}-gcc" ]; then
                export RISCV_TOOLCHAIN_PATH="$toolchain_dir"
                echo "Using local RISC-V toolchain from: $RISCV_TOOLCHAIN_PATH"
                break
            fi
            # Also check nested paths (e.g., tools_soc/opensrc/riscv-gnu-toolchain/...)
            nested_bin=$(find "$toolchain_dir" -type d -name "bin" -path "*/riscv-gnu-toolchain/*/bin" 2>/dev/null | head -1)
            if [ -n "$nested_bin" ] && [ -f "$nested_bin/${GCC_PREFIX}-gcc" ]; then
                # Use the parent directory of bin/ as the toolchain path
                export RISCV_TOOLCHAIN_PATH="$(dirname "$nested_bin")"
                echo "Using local RISC-V toolchain from: $RISCV_TOOLCHAIN_PATH"
                break
            fi
        fi
    done
fi

# If RISCV_TOOLCHAIN_PATH is set, add it to PATH
if [ -n "${RISCV_TOOLCHAIN_PATH:-}" ]; then
    # Check if toolchain has a bin/ subdirectory, otherwise use the directory directly
    if [ -d "${RISCV_TOOLCHAIN_PATH}/bin" ]; then
        export PATH="${RISCV_TOOLCHAIN_PATH}/bin:$PATH"
    else
        export PATH="${RISCV_TOOLCHAIN_PATH}:$PATH"
    fi

    # Check if local toolchain has libexec (needed for cc1)
    # If not, try to use the original toolchain location or set GCC_EXEC_PREFIX
    local libexec_path="${RISCV_TOOLCHAIN_PATH}/../libexec"
    if [ ! -d "$libexec_path" ]; then
        # Try to find the original toolchain location
        local original_toolchain="/tools_soc/opensrc/riscv-gnu-toolchain/2025.01.20-rhel-8.10"
        if [ -d "$original_toolchain/libexec" ]; then
            # Set GCC_EXEC_PREFIX to point to the original toolchain root
            export GCC_EXEC_PREFIX="$original_toolchain/"
            echo "Local toolchain incomplete, using libexec from: $original_toolchain"
        fi
    fi
fi

# Try to load toolchain via module if available and not already in PATH
if ! command -v ${GCC_PREFIX}-gcc &> /dev/null; then
    if command -v module &> /dev/null; then
        echo "Attempting to load RISC-V toolchain via module system..."
        module load riscv-gnu-toolchain/2025.01.20-rhel-8.10 2>/dev/null || true
    fi
fi

# Verify toolchain is available (unless SKIP_TOOLCHAIN_CHECK is set)
if [ "${SKIP_TOOLCHAIN_CHECK:-0}" != "1" ]; then
    if ! command -v ${GCC_PREFIX}-gcc &> /dev/null; then
        echo "ERROR: RISC-V toolchain not found!"
        echo "Please ensure ${GCC_PREFIX}-gcc is in your PATH"
        echo ""
        echo "Options to fix this:"
        echo "  1. Set RISCV_TOOLCHAIN_PATH environment variable:"
        echo "     export RISCV_TOOLCHAIN_PATH=/path/to/riscv-toolchain"
        echo "     $0 <command>"
        echo ""
        echo "  2. Add toolchain to PATH before running:"
        echo "     export PATH=/path/to/riscv-toolchain/bin:\$PATH"
        echo "     $0 <command>"
        echo ""
        echo "  3. Use environment modules:"
        echo "     module load riscv-gnu-toolchain/version"
        echo ""
        echo "Current GCC_PREFIX: $GCC_PREFIX"
        echo "Current PATH: $PATH"
        exit 1
    fi
else
    echo "WARNING: Skipping toolchain check (SKIP_TOOLCHAIN_CHECK=1)"
fi

# Verify perl is available
if ! command -v perl &> /dev/null; then
    echo "ERROR: perl not found! Please install perl."
    exit 1
fi

# ============================================================================
# Helper Functions
# ============================================================================

print_usage() {
    cat << EOF
Standalone SEP Firmware Build Script

Usage:
  $0 setup          - One-time setup (venv + perl config + picolibc)
  $0 venv           - Setup Python virtual environment only
  $0 config         - Run EL2 perl configuration only
  $0 picolibc       - Build picolibc only
  $0 build <test>   - Build a specific test (e.g., hello_world)
  $0 build-all      - Build all tests
  $0 clean <test>   - Clean a specific test
  $0 clean-all      - Clean all tests and build artifacts

Available tests:
  hello_world, aes_test, dma_test, efuse_sanity_csr_test, hmac_test, kmac_test,
  memory_sanity, otbn_loops_test, otbn_p256_verify_test,
  otbn_rsa_3072_verify_test, rom_sanity_test, uart,
  wdt_sanity_test

Environment variables:
  OCH_ROOT          - Repository root (auto-detected: $OCH_ROOT)
  RV_ROOT           - VeeR EL2 root (auto-set: $RV_ROOT)
  GCC_PREFIX        - RISC-V toolchain prefix (current: $GCC_PREFIX)

Examples:
  # First time setup
  $0 setup

  # Build hello_world test
  $0 build hello_world

  # Clean hello_world test
  $0 clean hello_world

  # Build all tests
  $0 build-all

  # Clean everything
  $0 clean-all

EOF
}

# ============================================================================
# Dependency Checkout
# ============================================================================

checkout_dependencies() {
    echo "============================================"
    echo "Checking out dependencies with Bender"
    echo "============================================"

    cd "$OCH_ROOT"

    # Check if bender is available
    if ! command -v bender &> /dev/null; then
        echo "Bender not found. Attempting to install via cargo..."

        # Check if cargo is available
        if ! command -v cargo &> /dev/null; then
            echo "ERROR: cargo (Rust) not found!"
            echo "Please install Rust from: https://rustup.rs/"
            echo ""
            echo "Or install bender manually from: https://github.com/pulp-platform/bender"
            exit 1
        fi

        echo "Installing bender with: cargo install bender --locked"
        cargo install bender --locked

        # Verify installation succeeded
        if ! command -v bender &> /dev/null; then
            echo "ERROR: bender installation failed!"
            echo "Please ensure ~/.cargo/bin is in your PATH"
            exit 1
        fi

        echo "✓ Bender installed successfully"
    fi

    bender checkout

    # Verify deps/el2 was created
    if [ ! -d "$RV_ROOT" ]; then
        echo "ERROR: deps/el2 directory not found after bender checkout"
        exit 1
    fi

    echo ""
    echo "✓ Dependencies checked out successfully"
    echo "  VeeR EL2 core available at: $RV_ROOT"
    echo ""
}

# ============================================================================
# EL2 Configuration (Perl)
# ============================================================================

configure_el2() {
    echo "============================================"
    echo "Configuring VeeR EL2 RISC-V Core for SEP"
    echo "============================================"

    # Check if RV_ROOT exists, if not run bender checkout
    if [ ! -d "$RV_ROOT" ]; then
        echo ""
        echo "VeeR EL2 dependency not found. Running bender checkout first..."
        echo ""
        checkout_dependencies
    fi

    # Check if EL2 snapshot already exists
    local snapshot_dir="$RV_ROOT/snapshots/sep"
    if [ -d "$snapshot_dir" ] && [ -f "$snapshot_dir/el2_param.vh" ]; then
        echo "EL2 configuration already exists. Skipping configuration."
        echo "  (To reconfigure, delete $snapshot_dir and run again)"
        echo ""
        return 0
    fi

    cd "$RV_ROOT"

    perl "$RV_ROOT/configs/veer.config" -snapshot=sep \
        -set=user_mode=1 \
        -set=btb_enabled=0 \
        -set=bht_size=32 \
        -set=dccm_enable=1 \
        -set=dccm_num_banks=2 \
        -set=dccm_region=0x0 \
        -set=dccm_offset=0x00000 \
        -set=dccm_size=64 \
        -set=dma_buf_depth=4 \
        -set=fast_interrupt_redirect=1 \
        -set=icache_enable=0 \
        -set=iccm_enable=1 \
        -set=iccm_region=0 \
        -set=iccm_offset=0x01000000 \
        -set=iccm_size=128 \
        -set=iccm_num_banks=4 \
        -set=lsu_stbuf_depth=4 \
        -set=lsu_num_nbload=4 \
        -set=load_to_use_plus1=0 \
        -set=pic_2cycle=1 \
        -set=pic_region=0 \
        -set=pic_offset=0x02000000 \
        -set=pic_size=32 \
        -set=pic_total_int=32 \
        -set=dma_buf_depth=4 \
        -set=timer_legal_en=1 \
        -set=bitmanip_zba=1 \
        -set=bitmanip_zbb=1 \
        -set=bitmanip_zbc=1 \
        -set=bitmanip_zbe=1 \
        -set=bitmanip_zbf=0 \
        -set=bitmanip_zbp=0 \
        -set=bitmanip_zbr=0 \
        -set=bitmanip_zbs=0 \
        -set=fpga_optimize=0 \
        -set=text_in_iccm=1 \
        -set=pmp_entries=16 \
        -set=smepmp=1 \
        -set=lockstep_enable=1 \
        -set=lockstep_regfile_enable=1 \
        -set=lockstep_delay=2

    echo ""
    echo "✓ EL2 configuration completed successfully"
    echo "  Generated files in: $RV_ROOT/snapshots/sep/"
    echo ""
}

# ============================================================================
# Python Virtual Environment Setup
# ============================================================================

setup_python_venv() {
    echo "============================================"
    echo "Setting up Python Virtual Environment"
    echo "============================================"

    cd "$OCH_ROOT"

    # Deactivate any existing venv
    deactivate 2>/dev/null || true

    # Create venv if it doesn't exist
    if [ ! -d "venv" ]; then
        echo "Creating Python virtual environment..."
        python3 -m venv "$OCH_ROOT/venv"
    else
        echo "Virtual environment already exists."
    fi

    # Activate venv
    source "$OCH_ROOT/venv/bin/activate"

    # Install requirements
    if [ -f "$OCH_ROOT/tools/requirements.txt" ]; then
        echo "Installing Python requirements..."
        pip3 install -r "$OCH_ROOT/tools/requirements.txt" --quiet
    else
        echo "WARNING: requirements.txt not found, installing essential tools..."
        pip3 install meson ninja --quiet
    fi

    echo ""
    echo "✓ Python virtual environment setup completed"
    echo "  Located at: $OCH_ROOT/venv/"
    echo ""
}

# ============================================================================
# Picolibc Build
# ============================================================================

build_picolibc() {
    echo "============================================"
    echo "Building Picolibc (RISC-V C Library)"
    echo "============================================"

    # Check if picolibc is already built
    local picolibc_install="$RV_ROOT/third_party/picolibc/install"
    local picolibc_specs="$picolibc_install/picolibc.specs"
    if [ -f "$picolibc_specs" ]; then
        echo "Picolibc already built. Skipping build."
        echo "  (To rebuild, delete $picolibc_install and run again)"
        echo ""
        return 0
    fi

    # Ensure Python venv is activated and PATH includes venv bin
    if [ -z "$VIRTUAL_ENV" ]; then
        echo "Activating Python virtual environment..."
        source "$OCH_ROOT/venv/bin/activate"
    fi

    # Make sure venv bin is in PATH for make subprocess
    export PATH="$OCH_ROOT/venv/bin:$PATH"

    cd "$RV_ROOT"

    if [ ! -f "$RV_ROOT/tools/picolibc.mk" ]; then
        echo "ERROR: picolibc.mk not found at $RV_ROOT/tools/picolibc.mk"
        exit 1
    fi

    # Verify meson is available
    if ! command -v meson &> /dev/null; then
        echo "ERROR: meson not found even after venv activation"
        echo "PATH: $PATH"
        exit 1
    fi

    # Verify toolchain is properly set up
    if ! command -v ${GCC_PREFIX}-gcc &> /dev/null; then
        echo "ERROR: RISC-V toolchain not found in PATH"
        echo "Please ensure ${GCC_PREFIX}-gcc is available"
        exit 1
    fi

    # Check if toolchain can actually compile (test with --version)
    if ! ${GCC_PREFIX}-gcc --version &> /dev/null; then
        echo "ERROR: RISC-V toolchain found but cannot execute"
        echo "This may indicate a broken or incomplete toolchain installation"
        exit 1
    fi

    make -f "$RV_ROOT/tools/picolibc.mk" all

    echo ""
    echo "✓ Picolibc built successfully"
    echo "  Installed in: $picolibc_install"
    echo ""
}

# ============================================================================
# Test Build
# ============================================================================

build_test() {
    local test_name=$1

    if [ -z "$test_name" ]; then
        echo "ERROR: Test name required"
        echo "Usage: $0 build <test_name>"
        exit 1
    fi

    local test_dir="$OCH_ROOT/dv/sep/tests/$test_name"

    if [ ! -d "$test_dir" ]; then
        echo "ERROR: Test directory not found: $test_dir"
        echo ""
        echo "Available tests:"
        ls -1 "$OCH_ROOT/dv/sep/tests" | grep -v "common"
        exit 1
    fi

    echo "============================================"
    echo "Building Test: $test_name"
    echo "============================================"

    cd "$test_dir"

    # Clean first
    make clean 2>/dev/null || true

    # Build
    make all

    echo ""
    echo "✓ Test '$test_name' built successfully"
    echo "  ELF file:  $test_dir/$test_name.elf"
    echo "  ITCM hex:  $test_dir/$test_name.itcm.hex"
    echo "  DTCM hex:  $test_dir/$test_name.dtcm.hex"
    echo "  Disassembly: $test_dir/$test_name.dis"
    echo ""
}

# ============================================================================
# Build All Tests
# ============================================================================

build_all_tests() {
    echo "============================================"
    echo "Building All SEP Tests"
    echo "============================================"
    echo ""

    local tests_dir="$OCH_ROOT/dv/sep/tests"
    local failed_tests=()
    local success_tests=()

    # Find all test directories (exclude common)
    for test_dir in "$tests_dir"/*/; do
        local test_name=$(basename "$test_dir")

        # Skip common directory
        if [ "$test_name" = "common" ]; then
            continue
        fi

        # Check if Makefile exists
        if [ ! -f "$test_dir/Makefile" ]; then
            echo "Skipping $test_name (no Makefile found)"
            continue
        fi

        echo "Building $test_name..."
        if build_test "$test_name" > /tmp/sep_build_${test_name}.log 2>&1; then
            success_tests+=("$test_name")
            echo "  ✓ $test_name - SUCCESS"
        else
            failed_tests+=("$test_name")
            echo "  ✗ $test_name - FAILED (see /tmp/sep_build_${test_name}.log)"
        fi
    done

    echo ""
    echo "============================================"
    echo "Build Summary"
    echo "============================================"
    echo "Successful: ${#success_tests[@]}"
    for test in "${success_tests[@]}"; do
        echo "  ✓ $test"
    done

    if [ ${#failed_tests[@]} -gt 0 ]; then
        echo ""
        echo "Failed: ${#failed_tests[@]}"
        for test in "${failed_tests[@]}"; do
            echo "  ✗ $test"
        done
        return 1
    fi

    echo ""
    echo "✓ All tests built successfully!"
}

# ============================================================================
# Clean Functions
# ============================================================================

clean_test() {
    local test_name=$1

    if [ -z "$test_name" ]; then
        echo "ERROR: Test name required"
        echo "Usage: $0 clean <test_name>"
        exit 1
    fi

    local test_dir="$OCH_ROOT/dv/sep/tests/$test_name"

    if [ ! -d "$test_dir" ]; then
        echo "ERROR: Test directory not found: $test_dir"
        exit 1
    fi

    echo "Cleaning test: $test_name"
    cd "$test_dir"
    make clean 2>/dev/null || true

    echo "✓ Test '$test_name' cleaned"
}

clean_all() {
    echo "============================================"
    echo "Cleaning All Tests and Build Artifacts"
    echo "============================================"

    # Clean all test directories
    local tests_dir="$OCH_ROOT/dv/sep/tests"
    for test_dir in "$tests_dir"/*/; do
        local test_name=$(basename "$test_dir")

        if [ "$test_name" = "common" ]; then
            # Clean common objects
            echo "Cleaning common test files..."
            cd "$test_dir"
            rm -f *.o *.su 2>/dev/null || true
        elif [ -f "$test_dir/Makefile" ]; then
            echo "Cleaning $test_name..."
            cd "$test_dir"
            make clean 2>/dev/null || true
        fi
    done

    echo ""
    echo "✓ All tests cleaned"
}

# ============================================================================
# Generate Register Files
# ============================================================================

generate_registers() {
    echo "============================================"
    echo "Generating Register Files"
    echo "============================================"

    # Check if register files already exist via sentinel written after successful generation
    local sentinel="$OCH_ROOT/.registers_generated"
    if [ -f "$sentinel" ]; then
        echo "Register files already exist. Skipping generation."
        echo "  (To regenerate, delete $sentinel and run again)"
        echo ""
        return 0
    fi

    # Ensure Python venv is activated
    if [ -z "$VIRTUAL_ENV" ]; then
        echo "Activating Python virtual environment..."
        source "$OCH_ROOT/venv/bin/activate"
    fi

    # Make sure venv bin is in PATH
    export PATH="$OCH_ROOT/venv/bin:$PATH"

    cd "$OCH_ROOT"

    if [ ! -f "$OCH_ROOT/tools/generate_all.py" ]; then
        echo "ERROR: generate_all.py not found at $OCH_ROOT/tools/generate_all.py"
        exit 1
    fi

    echo "Running register generation with axi4-lite interface..."
    python3 "$OCH_ROOT/tools/generate_all.py" -i axi4-lite

    touch "$sentinel"

    echo ""
    echo "✓ Register files generated successfully"
    echo ""
}

# ============================================================================
# Setup (One-time)
# ============================================================================

setup() {
    echo "============================================"
    echo "SEP Firmware Standalone Setup"
    echo "============================================"
    echo ""
    echo "This will:"
    echo "  1. Setup Python virtual environment"
    echo "  2. Generate register files"
    echo "  3. Configure VeeR EL2 RISC-V core"
    echo "  4. Build picolibc C library"
    echo ""
    echo "This only needs to be run once."
    echo ""

    # Setup Python venv
    setup_python_venv

    # Generate register files (needed before bender checkout)
    generate_registers

    # Configure EL2
    configure_el2

    # Build picolibc
    build_picolibc

    echo "============================================"
    echo "Setup Complete!"
    echo "============================================"
    echo ""
    echo "You can now build tests with:"
    echo "  $0 build hello_world"
    echo ""
}

# ============================================================================
# Main Script Logic
# ============================================================================

# Check if OCH_ROOT is valid
if [ ! -d "$OCH_ROOT/dv/sep/tests" ]; then
    echo "ERROR: Invalid OCH_ROOT: $OCH_ROOT"
    echo "Cannot find dv/sep/tests directory"
    exit 1
fi

# Parse command
case "${1:-}" in
    setup)
        setup
        ;;
    venv)
        setup_python_venv
        ;;
    config)
        configure_el2
        ;;
    picolibc)
        build_picolibc
        ;;
    build)
        if [ -z "${2:-}" ]; then
            echo "ERROR: Test name required"
            print_usage
            exit 1
        fi
        # Ensure venv is activated for builds
        if [ -z "$VIRTUAL_ENV" ] && [ -d "$OCH_ROOT/venv" ]; then
            source "$OCH_ROOT/venv/bin/activate"
        fi
        build_test "$2"
        ;;
    build-all)
        # Ensure venv is activated for builds
        if [ -z "$VIRTUAL_ENV" ] && [ -d "$OCH_ROOT/venv" ]; then
            source "$OCH_ROOT/venv/bin/activate"
        fi
        build_all_tests
        ;;
    clean)
        if [ -z "${2:-}" ]; then
            echo "ERROR: Test name required"
            print_usage
            exit 1
        fi
        clean_test "$2"
        ;;
    clean-all)
        clean_all
        ;;
    help|--help|-h)
        print_usage
        ;;
    *)
        echo "ERROR: Invalid command: ${1:-}"
        echo ""
        print_usage
        exit 1
        ;;
esac

exit 0
