#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# =============================================================================
# vp_test_env.sh — shared environment discovery for run_test.sh / run_all_tests.sh
#
# Sourced, not executed.  Sets up everything that differs between the original
# tt-oca-hw checkout and this copy under sw/sep-vp-tests/sw-tests-from-tt-oca-hw:
#
#   OCH_ROOT   root of this mini tree (holds fw/, plus meta/ dv/ vendor/ deps/
#              symlinks into dependencies/)
#   RV_ROOT    VeeR EL2 / picolibc location ($OCH_ROOT/deps/el2)
#   GCC_PREFIX RISC-V toolchain prefix without the trailing dash, detected from
#              PATH so the same tree builds on macOS, Ubuntu and RHEL
#   SEP_VP     sep-vp executable, found by walking up to the tt-oca-harness-model root
#   CONFIG     accellera_config.ini that goes with it
#
# Every value can be overridden by exporting it before calling the runners.
# =============================================================================

# tests/ -> sep/ -> fw/ -> tree root
VP_TESTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export OCH_ROOT="${OCH_ROOT:-$(cd "${VP_TESTS_DIR}/../../.." && pwd)}"
export RV_ROOT="${RV_ROOT:-${OCH_ROOT}/deps/el2}"

# -----------------------------------------------------------------------------
# RISC-V toolchain
#
# common.mk defaults GCC_PREFIX to riscv64-unknown-elf, which only exists on the
# Ubuntu/RHEL machines.  Homebrew installs riscv64-elf-, so probe instead.
# -----------------------------------------------------------------------------
vp_detect_toolchain() {
    local candidates="riscv64-unknown-elf riscv64-elf riscv-none-elf riscv64-linux-gnu riscv-none-embed"
    local p

    if [ -n "${RISCV_TOOLCHAIN_PATH:-}" ]; then
        if [ -d "${RISCV_TOOLCHAIN_PATH}/bin" ]; then
            export PATH="${RISCV_TOOLCHAIN_PATH}/bin:${PATH}"
        else
            export PATH="${RISCV_TOOLCHAIN_PATH}:${PATH}"
        fi
    fi

    if [ -n "${GCC_PREFIX:-}" ] && command -v "${GCC_PREFIX}-gcc" >/dev/null 2>&1; then
        export GCC_PREFIX
        return 0
    fi

    for p in ${candidates}; do
        if command -v "${p}-gcc" >/dev/null 2>&1; then
            export GCC_PREFIX="${p}"
            return 0
        fi
    done

    echo "ERROR: no RISC-V toolchain found in PATH." >&2
    echo "  Tried: ${candidates}" >&2
    echo "  macOS:   brew install riscv64-elf-gcc" >&2
    echo "  Ubuntu:  sudo apt install gcc-riscv64-unknown-elf" >&2
    echo "  RHEL/TT: module load riscv-gnu-toolchain/2025.01.20-rhel-8.10" >&2
    echo "  Or set GCC_PREFIX / RISCV_TOOLCHAIN_PATH." >&2
    return 1
}

# -----------------------------------------------------------------------------
# Vendored dependencies
#
# picolibc is built once by dependencies/setup_dependencies.sh.  Run it
# automatically so a fresh checkout only needs ./run_all_tests.sh.
# -----------------------------------------------------------------------------
vp_ensure_dependencies() {
    local setup="${OCH_ROOT}/dependencies/setup_dependencies.sh"
    local install specs

    # RV_ROOT reaches the install through the deps/el2 symlink; the specs file
    # spells it out physically, so compare like with like.
    install="$(cd "${RV_ROOT}/third_party/picolibc/install" 2>/dev/null && pwd -P)" || install=""
    specs="${install:-/nonexistent}/picolibc.specs"

    # picolibc.specs carries absolute -isystem and -L paths from build time, so
    # an existing file is not enough: after a rename or a move it points into
    # thin air and the link fails with "cannot find -lc".  Re-run the setup,
    # which rewrites the paths without rebuilding.
    if [ -f "${specs}" ] && grep -q "${install}" "${specs}"; then
        return 0
    fi

    if [ ! -x "${setup}" ]; then
        echo "ERROR: picolibc is missing and ${setup} was not found." >&2
        return 1
    fi

    echo "[vp_test_env] setting up dependencies (picolibc)"
    "${setup}" || return 1
}

# -----------------------------------------------------------------------------
# sep-vp
#
# The original scripts counted directory levels up to the repo root.  This copy
# sits one level deeper, so walk up until vp/ shows up instead.
# -----------------------------------------------------------------------------
vp_find_sep_vp() {
    local dir="${VP_TESTS_DIR}" cand mtime newest="" newest_mtime=0

    if [ -n "${SEP_VP:-}" ] && [ -x "${SEP_VP}" ]; then
        VP_ROOT="${VP_ROOT:-$(cd "$(dirname "${SEP_VP}")/../.." && pwd)}"
    else
        # Prefer the newest built sep-vp. vp/build/bin/sep-vp is often months
        # stale and lacks peripherals (sep_reset_ctrl at 0x10803000, ABR, …)
        # that firmware tests walk on first access; a store there traps with
        # mcause=7. run_sep_vp_tests.sh uses the same newest-mtime rule.
        while [ "${dir}" != "/" ]; do
            for cand in "${dir}/vp/build_sep/bin/sep-vp" "${dir}/vp/build/bin/sep-vp"; do
                if [ -x "${cand}" ]; then
                    mtime="$(stat -f %m "${cand}" 2>/dev/null || stat -c %Y "${cand}" 2>/dev/null || echo 0)"
                    if [ "${mtime}" -ge "${newest_mtime}" ]; then
                        newest="${cand}"
                        newest_mtime="${mtime}"
                        VP_ROOT="${dir}"
                    fi
                fi
            done
            # Remember the repo root even if sep-vp has not been built yet.
            [ -d "${dir}/vp/platform/sep" ] && VP_ROOT="${dir}"
            dir="$(dirname "${dir}")"
        done
        [ -n "${newest}" ] && SEP_VP="${newest}"
    fi

    if [ -z "${SEP_VP:-}" ] || [ ! -x "${SEP_VP}" ]; then
        echo "ERROR: sep-vp not found above ${VP_TESTS_DIR}" >&2
        echo "  Build it (cd vp && ./configure_vp.sh && cmake --build build --target sep-vp)" >&2
        echo "  or export SEP_VP=/path/to/sep-vp" >&2
        return 1
    fi

    CONFIG="${CONFIG:-${VP_ROOT}/vp/platform/sep/config/accellera_config.ini}"
    if [ ! -f "${CONFIG}" ]; then
        echo "ERROR: VP config not found: ${CONFIG}" >&2
        return 1
    fi

    export SEP_VP CONFIG VP_ROOT
    return 0
}

# -----------------------------------------------------------------------------
# Per-test knobs that used to live in bash-4 associative arrays.  macOS ships
# bash 3.2, which has none, so they are plain lookups here.
# -----------------------------------------------------------------------------

# OTBN tests need the VP's ISS pointed at a specific algorithm model.
vp_algo_override() {
    case "$1" in
        otbn_loops_test|otbn_plic_test|otbn_sw_error_test) echo "otbn_loop" ;;
        otbn_smoke_test|otbn_sep_integration_test|otbn_fw_control_test) echo "smoke" ;;
        otbn_p256_verify_test)     echo "p256_ecdsa" ;;
        otbn_rsa_3072_verify_test) echo "rsa_3072" ;;
        *) echo "" ;;
    esac
}

# Which base config a test needs.
#
# accellera_config.ini preloads the SPI flash with the boot image, because the
# ROM boot flow needs something to boot from. The flash model programs the way
# real flash does — it can only clear bits, never set them — so a test that
# writes its own pattern over preloaded bytes reads back `pattern & boot_byte`
# and sees garbage rather than what it wrote. In the RTL testbench the flash
# starts erased, so upstream never has to think about this.
# The eFuse array is preloaded with default_efuse.preload, the same image
# sep_test_template loads via +sep_preload_efuse. The three sep_efuse_fw_* tests
# override their run_flags with +SEP_EFUSE_NO_PRELOAD instead, because they program
# fuses themselves and have to start from a blank part: a preloaded field cannot be
# programmed to a different value, since fuses only go 0->1.
vp_base_config() {
    case "$1" in
        spi_ot_flash_write_read_test|spi_ot_flash_dual_read_test|spi_ot_flash_quad_read_test)
            echo "$(dirname "${CONFIG}")/accellera_config_no_spipreload.ini" ;;
        sep_efuse_fw_otp_rw_test|sep_efuse_fw_shadow_rw_test|sep_efuse_fw_token_match_test)
            vp_efuse_blank_config ;;
        *) echo "${CONFIG}" ;;
    esac
}

# The +SEP_EFUSE_NO_PRELOAD equivalent: echoes a config whose fuse array starts erased.
#
# fuse_preload_file lives in efuse_vp.ini, which accellera_config.ini @includes, so
# dropping it takes a copy of both — the include with that line commented out, and a
# top-level config pointing at the copy. Both are derived with sed on each call rather
# than checked in, so they track edits to the shared config instead of drifting from it,
# which is also why accellera_config_no_spipreload.ini is built that way.
vp_efuse_blank_config() {
    local dir top_blank
    dir="$(dirname "${CONFIG}")"
    top_blank="${dir}/accellera_config_efuse_blank.ini"
    sed 's|^och_sep_ss1\.sep_efuse\.fuse_preload_file|#&|' \
        "${dir}/efuse_vp.ini" > "${dir}/efuse_vp_blank.ini"
    sed -E 's|^@include efuse_vp\.ini[[:space:]]*$|@include efuse_vp_blank.ini|' \
        "${CONFIG}" > "${top_blank}"
    echo "${top_blank}"
}

# Tests whose Makefile emits a second ELF from the same sources.
vp_extra_elfs() {
    case "$1" in
        spi_crc_test)     echo "spi_crc_ot_mode" ;;
        spi_phy_reg_test) echo "spi_phy_reg_ot_mode" ;;
        *) echo "" ;;
    esac
}

# Reverse of the above: which directory holds a variant ELF.
vp_test_dir_for() {
    case "$1" in
        spi_crc_ot_mode)     echo "spi_crc_test" ;;
        spi_phy_reg_ot_mode) echo "spi_phy_reg_test" ;;
        *) echo "$1" ;;
    esac
}

# Write a copy of CONFIG with the OTBN algorithm patched in; echoes its path.
vp_make_test_config() {
    local algo="$1" base="${2:-${CONFIG}}" config_dir tmp
    config_dir="$(cd "$(dirname "${base}")" && pwd)"
    tmp="$(mktemp /tmp/sep_vp_config_XXXXXX.ini)"
    sed -e "s|algorithm_type *:.*|algorithm_type : ${algo}|g" \
        -e "s|configFile *:.*|configFile : ${config_dir}/veeriss_config.json|g" \
        "${base}" > "${tmp}"
    echo "${tmp}"
}
