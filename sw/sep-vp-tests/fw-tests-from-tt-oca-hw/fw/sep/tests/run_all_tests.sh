#!/usr/bin/env bash
# run_all_tests.sh — Build and run all VP firmware tests, report pass/fail counts.
#
# Usage:
#   ./run_all_tests.sh              # build + run all tests (default)
#   ./run_all_tests.sh --clean      # clean + build + run all tests
#   ./run_all_tests.sh --no-build   # run only (use existing ELFs)
#   ./run_all_tests.sh -t 60        # override per-test timeout (default 25s)
#   ./run_all_tests.sh -v           # verbose: show [VP]/PASS/FAIL lines per test
#
# Each test: sep-vp is launched, output is monitored for the PASSED/FAILED line,
# then the process is killed (Ctrl+C equivalent) before starting the next test.
# Tests that produce no result within TIMEOUT seconds are killed and marked STUCK.

set -uo pipefail

# ---------------------------------------------------------------------------
# Paths — OCH_ROOT, RV_ROOT, GCC_PREFIX, SEP_VP and CONFIG come from the shared
# helper, which locates sep-vp by walking up instead of counting directories.
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=vp_test_env.sh
source "${SCRIPT_DIR}/vp_test_env.sh"

LOGS_DIR="$SCRIPT_DIR/logs"

# ---------------------------------------------------------------------------
# Excluded directories
# ---------------------------------------------------------------------------
EXCLUDE=("bl1_pass_test" "common" "common_otbn" "logs")

# Per-test OTBN algorithm overrides and multi-ELF variants live in
# vp_test_env.sh as vp_algo_override / vp_extra_elfs — plain lookups rather than
# associative arrays, which macOS bash 3.2 does not have.

# Scratch config written per-test when an override is needed; cleaned up after.
TEMP_CONFIG=""

cleanup_temp_config() {
    if [[ -n "$TEMP_CONFIG" && -f "$TEMP_CONFIG" ]]; then
        rm -f "$TEMP_CONFIG"
        TEMP_CONFIG=""
    fi
}

# ---------------------------------------------------------------------------
# Defaults / option parsing
# ---------------------------------------------------------------------------
TIMEOUT=25   # seconds before declaring a test STUCK
VERBOSE=0
BUILD=1      # build before run by default
CLEAN=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        -t|--timeout)  TIMEOUT="$2"; shift 2 ;;
        -v|--verbose)  VERBOSE=1;    shift   ;;
        --clean)       CLEAN=1;      shift   ;;
        --no-build)    BUILD=0;      shift   ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# ---------------------------------------------------------------------------
# Counters and result lists
# ---------------------------------------------------------------------------
total=0; passed=0; failed=0; stuck=0; skipped=0; excluded=0
declare -a PASSED_TESTS=()
declare -a FAILED_TESTS=()
declare -a STUCK_TESTS=()

# Global: PID of the currently running sep-vp (used by trap and run_test)
CURRENT_VP_PID=""
# Global: result written by run_test (avoids subshell capture problems)
TEST_RESULT=""

# ---------------------------------------------------------------------------
# Cleanup on Ctrl+C / SIGTERM
# ---------------------------------------------------------------------------
cleanup() {
    echo ""
    echo "Interrupted — killing active sep-vp..."
    if [[ -n "$CURRENT_VP_PID" ]]; then
        kill -KILL "$CURRENT_VP_PID" 2>/dev/null || true
        CURRENT_VP_PID=""
    fi
    cleanup_temp_config
    print_summary
    exit 130
}
trap cleanup INT TERM

# ---------------------------------------------------------------------------
# Prerequisite checks
# ---------------------------------------------------------------------------
vp_find_sep_vp || exit 1

if [[ "$BUILD" -eq 1 ]]; then
    vp_detect_toolchain || exit 1
    vp_ensure_dependencies || exit 1
fi

mkdir -p "$LOGS_DIR"

echo "============================================================"
echo " VP Firmware Test Runner"
echo "============================================================"
printf " VP binary   : %s\n" "$SEP_VP"
printf " Config      : %s\n" "$CONFIG"
printf " Logs dir    : %s\n" "$LOGS_DIR"
printf " Stuck after : %ss\n" "$TIMEOUT"
if [[ "$BUILD" -eq 1 ]]; then
    [[ "$CLEAN" -eq 1 ]] && printf " Build       : clean + build\n" || printf " Build       : incremental\n"
else
    printf " Build       : skipped (--no-build)\n"
fi
echo "============================================================"
echo ""

# ---------------------------------------------------------------------------
# run_test <test_name> <elf>
#   Sets global TEST_RESULT to: PASSED | FAILED | STUCK | CRASH
#   Polls for the result banner and hard-kills the VP once TIMEOUT seconds
#   elapse. Deliberately not timeout(1): that is GNU coreutils and is absent on
#   macOS, so the deadline is enforced in-shell instead.
# ---------------------------------------------------------------------------
run_test() {
    local test_name="$1"
    local elf="$2"
    local logfile="$LOGS_DIR/${test_name}.log"
    TEST_RESULT="STUCK"

    # Select config: use a patched temp config if this test has an algo override.
    local run_config="$CONFIG"
    local algo
    algo="$(vp_algo_override "$test_name")"
    if [[ -n "$algo" ]]; then
        TEMP_CONFIG="$(vp_make_test_config "$algo")"
        run_config="$TEMP_CONFIG"
    fi

    # The VP keeps running after the firmware finishes, so it is killed as soon
    # as the banner appears.  macOS has no timeout(1); the deadline is enforced
    # by the poll loop instead.
    "$SEP_VP" "$run_config" "$elf" >"$logfile" 2>&1 &
    CURRENT_VP_PID=$!

    SECONDS=0
    while kill -0 "$CURRENT_VP_PID" 2>/dev/null; do
        if grep -q "\[VP\] SIMULATION OF THE TEST PASSED" "$logfile" 2>/dev/null; then
            TEST_RESULT="PASSED"
            break
        fi
        if grep -q "\[VP\] SIMULATION OF THE TEST FAILED" "$logfile" 2>/dev/null; then
            TEST_RESULT="FAILED"
            break
        fi
        if [[ "$SECONDS" -ge "$TIMEOUT" ]]; then
            break
        fi
        sleep 0.3
    done

    # Killing the VP on purpose still makes the shell announce "Terminated" for
    # the job, at the first command boundary after the signal lands, so the whole
    # teardown runs with stderr closed rather than just the wait.
    {
        if kill -0 "$CURRENT_VP_PID" 2>/dev/null; then
            kill -TERM "$CURRENT_VP_PID" 2>/dev/null || true
            sleep 1
            kill -KILL "$CURRENT_VP_PID" 2>/dev/null || true
        fi
        wait "$CURRENT_VP_PID" || true
    } 2>/dev/null
    CURRENT_VP_PID=""

    # One final grep in case the result line appeared right at exit
    if [[ "$TEST_RESULT" == "STUCK" ]]; then
        if grep -q "\[VP\] SIMULATION OF THE TEST PASSED" "$logfile" 2>/dev/null; then
            TEST_RESULT="PASSED"
        elif grep -q "\[VP\] SIMULATION OF THE TEST FAILED" "$logfile" 2>/dev/null; then
            TEST_RESULT="FAILED"
        elif ! grep -q "." "$logfile" 2>/dev/null; then
            TEST_RESULT="CRASH"   # process died immediately with no output
        fi
    fi

    cleanup_temp_config

    if [[ "$VERBOSE" -eq 1 ]]; then
        echo ""
        grep -E "(\[VP\]|=== SPI|=== DMA|PASS|FAIL|ERROR)" "$logfile" 2>/dev/null || true
        echo ""
    fi
}

# ---------------------------------------------------------------------------
# Print summary (called at end and on interrupt)
# ---------------------------------------------------------------------------
print_summary() {
    local run=$((passed + failed + stuck))
    echo ""
    echo "============================================================"
    echo " SUMMARY"
    echo "============================================================"
    printf " Tests run    : %d\n" "$run"
    printf " PASSED       : %d\n" "$passed"
    printf " FAILED       : %d\n" "$failed"
    printf " STUCK/CRASH  : %d\n" "$stuck"
    printf " Skipped      : %d  (build failed or no ELF)\n" "$skipped"
    printf " Excluded     : %d  (%s)\n" "$excluded" "${EXCLUDE[*]}"
    if [[ $run -gt 0 ]]; then
        printf " Pass rate    : %d%%  (%d / %d)\n" \
               "$((passed * 100 / run))" "$passed" "$run"
    fi
    echo "============================================================"
    echo ""

    if [[ ${#PASSED_TESTS[@]} -gt 0 ]]; then
        printf "PASSED (%d):\n" "${#PASSED_TESTS[@]}"
        for t in "${PASSED_TESTS[@]}"; do printf "  [PASS]  %s\n" "$t"; done
        echo ""
    fi
    if [[ ${#FAILED_TESTS[@]} -gt 0 ]]; then
        printf "FAILED (%d):\n" "${#FAILED_TESTS[@]}"
        for t in "${FAILED_TESTS[@]}"; do
            printf "  [FAIL]  %-50s  → %s/%s.log\n" "$t" "$LOGS_DIR" "$t"
        done
        echo ""
    fi
    if [[ ${#STUCK_TESTS[@]} -gt 0 ]]; then
        printf "STUCK / CRASH (%d):\n" "${#STUCK_TESTS[@]}"
        for t in "${STUCK_TESTS[@]}"; do
            printf "  [STUCK] %-50s  → %s/%s.log\n" "$t" "$LOGS_DIR" "$t"
        done
        echo ""
    fi
    printf "Full logs: %s\n" "$LOGS_DIR"
}

# ---------------------------------------------------------------------------
# Main loop
# ---------------------------------------------------------------------------
for test_dir in "$SCRIPT_DIR"/*/; do
    [[ -d "$test_dir" ]] || continue
    test_name=$(basename "$test_dir")

    # Skip excluded directories
    is_excluded=0
    for excl in "${EXCLUDE[@]}"; do
        [[ "$test_name" == "$excl" ]] && { is_excluded=1; break; }
    done
    if (( is_excluded )); then
        (( excluded += 1 )) || true
        continue
    fi

    # Build if requested (default: yes)
    if [[ "$BUILD" -eq 1 ]]; then
        build_log="$LOGS_DIR/${test_name}.build.log"
        if [[ "$CLEAN" -eq 1 ]]; then
            (cd "$test_dir" && make clean) >/dev/null 2>&1 || true
        fi
        if ! (cd "$test_dir" && make -s) >"$build_log" 2>&1; then
            printf "  [SKIP] %-55s (build failed — see %s)\n" "$test_name" "$build_log"
            (( skipped += 1 )) || true
            continue
        fi
    fi

    # Skip if ELF missing (not yet compiled)
    elf="${test_dir}${test_name}.elf"
    if [[ ! -f "$elf" ]]; then
        printf "  [SKIP] %-55s (no ELF)\n" "$test_name"
        (( skipped += 1 )) || true
        continue
    fi

    (( total += 1 )) || true
    printf "  [%3d] %-55s" "$total" "$test_name"

    run_test "$test_name" "$elf"   # sets TEST_RESULT directly — no subshell

    case "$TEST_RESULT" in
        PASSED)
            printf "  [VP] SIMULATION OF THE TEST PASSED\n"
            PASSED_TESTS+=("$test_name")
            (( passed += 1 )) || true
            ;;
        FAILED)
            printf "  [VP] SIMULATION OF THE TEST FAILED\n"
            FAILED_TESTS+=("$test_name")
            (( failed += 1 )) || true
            ;;
        STUCK)
            printf "  STUCK (>%ss — killed)\n" "$TIMEOUT"
            STUCK_TESTS+=("$test_name")
            (( stuck += 1 )) || true
            ;;
        CRASH)
            printf "  CRASH (no output)\n"
            STUCK_TESTS+=("$test_name")
            (( stuck += 1 )) || true
            ;;
    esac

    # Run any extra ELF variants (e.g. OT-mode variants compiled with -DUSE_OT_SPI)
    extra_stem="$(vp_extra_elfs "$test_name")"
    if [[ -n "$extra_stem" ]]; then
        extra_elf="${test_dir}${extra_stem}.elf"
        if [[ -f "$extra_elf" ]]; then
            (( total += 1 )) || true
            printf "  [%3d] %-55s" "$total" "$extra_stem"
            run_test "$extra_stem" "$extra_elf"
            case "$TEST_RESULT" in
                PASSED)
                    printf "  [VP] SIMULATION OF THE TEST PASSED\n"
                    PASSED_TESTS+=("$extra_stem")
                    (( passed += 1 )) || true
                    ;;
                FAILED)
                    printf "  [VP] SIMULATION OF THE TEST FAILED\n"
                    FAILED_TESTS+=("$extra_stem")
                    (( failed += 1 )) || true
                    ;;
                STUCK)
                    printf "  STUCK (>%ss — killed)\n" "$TIMEOUT"
                    STUCK_TESTS+=("$extra_stem")
                    (( stuck += 1 )) || true
                    ;;
                CRASH)
                    printf "  CRASH (no output)\n"
                    STUCK_TESTS+=("$extra_stem")
                    (( stuck += 1 )) || true
                    ;;
            esac
        else
            printf "  [SKIP] %-55s (no ELF: %s)\n" "$extra_stem" "$extra_elf"
            (( skipped += 1 )) || true
        fi
    fi
done

print_summary
