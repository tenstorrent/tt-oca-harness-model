#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# sw/smu-vp-tests/run_smu_vp_tests.sh
#
# Build and run the SMU platform tests on smu-vp (SMC + SEP integrated over
# the SMU on-die interconnect).  Mirrors the shape of
# sw/smc-vp-tests/run_smc_vp_tests.sh.
#
# Each test builds two firmware halves (SMC RV64 + SEP RV32) and runs them
# concurrently on smu-vp.  A test passes when BOTH halves print their PASS
# banner (SMC on UART0, SEP on the virtconsole) and neither prints FAIL.
#
# Usage:
#   ./run_smu_vp_tests.sh              # all tests
#   ./run_smu_vp_tests.sh <test-dir>   # a single test (e.g. smu-link-test)
#
# Environment overrides:
#   VP            path to smu-vp      (default: ../../vp/build_smc/bin/smu-vp)
#   SMC_INI       SMC-side ini        (default: vp/platform/smu/config/smc_smu_vp.ini)
#   SEP_INI       SEP-side ini        (default: vp/platform/smu/config/sep_smu_config.ini)
#   SIM_TIME_MS   simulation window   (default: 50)
#   RISCV_PREFIX  toolchain prefix    (auto-detected: riscv64-unknown-elf- / riscv64-elf- / ...)

set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

VP="${VP:-$SCRIPT_DIR/../../vp/build_smc/bin/smu-vp}"
SMC_INI="${SMC_INI:-$SCRIPT_DIR/../../vp/platform/smu/config/smc_smu_vp.ini}"
SEP_INI="${SEP_INI:-$SCRIPT_DIR/../../vp/platform/smu/config/sep_smu_config.ini}"
SIM_TIME_MS="${SIM_TIME_MS:-50}"

if [ -n "${RISCV_TOOLCHAIN_PATH:-}" ]; then
    export PATH="${RISCV_TOOLCHAIN_PATH}/bin:${PATH}"
fi

if [ -z "${RISCV_PREFIX:-}" ]; then
    for p in riscv64-unknown-elf- riscv64-elf- riscv-none-elf-; do
        if command -v "${p}gcc" >/dev/null 2>&1; then
            RISCV_PREFIX="$p"
            break
        fi
    done
    RISCV_PREFIX="${RISCV_PREFIX:-riscv64-unknown-elf-}"
fi

if [ ! -x "$VP" ]; then
    echo "ERROR: smu-vp not found/executable at '$VP' (set VP=...)" >&2
    echo "  cmake -S vp -B vp/build_smc && cmake --build vp/build_smc --target smu-vp" >&2
    exit 2
fi
if ! command -v "${RISCV_PREFIX}gcc" >/dev/null 2>&1; then
    echo "ERROR: RISC-V toolchain '${RISCV_PREFIX}gcc' not found (set RISCV_PREFIX=...)" >&2
    exit 2
fi

if [ $# -gt 0 ]; then
    TESTS="$*"
else
    TESTS=$(ls -d */ | grep -v '^common' | tr -d '/')
fi

pass=0
fail=0
failed_tests=""

for t in $TESTS; do
    [ -f "$t/Makefile" ] || continue
    echo "======================================================================"
    echo "SMU-VP TEST: $t"
    echo "======================================================================"

    if ! make -C "$t" -s RISCV_PREFIX="$RISCV_PREFIX" >/dev/null; then
        echo "RESULT: $t: BUILD FAILED"
        fail=$((fail + 1)); failed_tests="$failed_tests $t(build)"
        continue
    fi

    smc_elf=$(find "$t" -maxdepth 1 -type f -name 'smu_*_smc' | head -n1)
    sep_elf=$(find "$t" -maxdepth 1 -type f -name 'smu_*_sep' | head -n1)
    if [ -z "$smc_elf" ] || [ -z "$sep_elf" ]; then
        echo "RESULT: $t: FAIL (missing smu_*_smc / smu_*_sep ELF after build)"
        fail=$((fail + 1)); failed_tests="$failed_tests $t"
        continue
    fi

    log=$(mktemp -t smu_vp_test.XXXXXX)
    ( cd "$t" && "$VP" "$SMC_INI" "$(basename "$smc_elf")" \
        "$SEP_INI" "$(basename "$sep_elf")" "$SIM_TIME_MS" ) >"$log" 2>&1

    smc_pass=$(grep -c "PASS: .*SMC side" "$log")
    sep_pass=$(grep -c "PASS: .*SEP side" "$log")
    any_fail=$(grep -c "FAIL:" "$log")

    if [ "$smc_pass" -ge 1 ] && [ "$sep_pass" -ge 1 ] && [ "$any_fail" -eq 0 ]; then
        echo "RESULT: $t: PASS"
        pass=$((pass + 1))
    else
        echo "RESULT: $t: FAIL (smc_pass=$smc_pass sep_pass=$sep_pass fails=$any_fail) — log:"
        cat "$log"
        fail=$((fail + 1)); failed_tests="$failed_tests $t"
    fi
    rm -f "$log"
done

echo "======================================================================"
echo "SMU-VP TEST SUMMARY: $pass passed, $fail failed"
if [ "$fail" -ne 0 ]; then
    echo "Failed tests:$failed_tests"
    exit 1
fi
echo "All SMU-VP tests PASSED"
