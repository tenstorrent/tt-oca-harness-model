#!/usr/bin/env bash
# run_test.sh — Build and run a single VP firmware test by name.
#
# Usage:
#   ./run_test.sh <test_name>                # build + run
#   ./run_test.sh <test_name> --run-only     # skip build, run existing ELF
#   ./run_test.sh <test_name> --build-only   # build only, do not run
#   ./run_test.sh <test_name> -t 60          # override timeout (default 25s)
#
# Runs from ANY directory — all paths are resolved from the script location.
# Does NOT require sourcing setup_fw_env.sh first; this script sets up the
# environment internally.
#
# Requirements:
#   - a RISC-V bare-metal toolchain in PATH (any of riscv64-unknown-elf-,
#     riscv64-elf-, riscv-none-elf-); see vp_test_env.sh
#   - sep-vp built somewhere above this directory (vp/build/bin/sep-vp)

set -uo pipefail

# ---------------------------------------------------------------------------
# Environment — OCH_ROOT, RV_ROOT, GCC_PREFIX, SEP_VP and CONFIG all come from
# the shared helper so this script and run_all_tests.sh cannot drift apart.
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=vp_test_env.sh
source "${SCRIPT_DIR}/vp_test_env.sh"

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
if [[ $# -lt 1 ]]; then
    echo "Usage: $(basename "$0") <test_name> [--run-only] [--build-only] [-t <timeout>]"
    echo ""
    echo "Available tests:"
    for d in "$SCRIPT_DIR"/*/; do
        name=$(basename "$d")
        [[ "$name" == "common" || "$name" == "common_otbn" || "$name" == "logs" ]] && continue
        printf "  %s\n" "$name"
    done
    exit 1
fi

TEST_NAME="$1"; shift

BUILD=1
RUN=1
TIMEOUT=25

while [[ $# -gt 0 ]]; do
    case "$1" in
        --run-only)    BUILD=0; shift ;;
        --build-only)  RUN=0;   shift ;;
        -t|--timeout)  TIMEOUT="$2"; shift 2 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# ---------------------------------------------------------------------------
# Validate test directory
# ---------------------------------------------------------------------------
# Some tests produce multiple ELF variants that live in the parent test
# directory, so a variant name maps back to its parent directory.
TEST_DIR="$SCRIPT_DIR/$(vp_test_dir_for "$TEST_NAME")"

if [[ ! -d "$TEST_DIR" ]]; then
    echo "ERROR: Test directory not found: $TEST_DIR"
    echo ""
    echo "Available tests:"
    for d in "$SCRIPT_DIR"/*/; do
        name=$(basename "$d")
        [[ "$name" == "common" || "$name" == "common_otbn" || "$name" == "logs" ]] && continue
        printf "  %s\n" "$name"
    done
    exit 1
fi

ELF="$TEST_DIR/${TEST_NAME}.elf"

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
if [[ "$BUILD" -eq 1 ]]; then
    vp_detect_toolchain || exit 1
    vp_ensure_dependencies || exit 1

    echo "============================================================"
    echo " Building: $TEST_NAME"
    echo " OCH_ROOT: $OCH_ROOT"
    echo " RV_ROOT : $RV_ROOT"
    echo " Toolchain: ${GCC_PREFIX}-gcc"
    echo "============================================================"

    (cd "$TEST_DIR" && make) || {
        echo ""
        echo "BUILD FAILED: $TEST_NAME"
        exit 1
    }

    echo ""
    echo "BUILD OK: $ELF"
fi

# ---------------------------------------------------------------------------
# Run
# ---------------------------------------------------------------------------
if [[ "$RUN" -eq 0 ]]; then
    exit 0
fi

if [[ ! -f "$ELF" ]]; then
    echo "ERROR: ELF not found: $ELF"
    echo "  Build it first with:  $(basename "$0") $TEST_NAME --build-only"
    exit 1
fi

vp_find_sep_vp || exit 1

# Select config — patch algorithm_type for OTBN tests
RUN_CONFIG="$CONFIG"
TEMP_CONFIG=""

ALGO="$(vp_algo_override "$TEST_NAME")"
if [[ -n "$ALGO" ]]; then
    TEMP_CONFIG="$(vp_make_test_config "$ALGO")"
    RUN_CONFIG="$TEMP_CONFIG"
    echo "Using OTBN algorithm override: $ALGO"
fi

cleanup() {
    [[ -n "$TEMP_CONFIG" && -f "$TEMP_CONFIG" ]] && rm -f "$TEMP_CONFIG"
}
trap cleanup EXIT

echo "============================================================"
echo " Running: $TEST_NAME"
echo " ELF    : $ELF"
echo " VP     : $SEP_VP"
echo " Timeout: ${TIMEOUT}s"
echo "============================================================"

mkdir -p logs

# BSD mktemp only substitutes X's at the end of the template, so name the log
# after the test instead of trying to be unique.
LOGFILE="logs/${TEST_NAME}.log"

RESULT="STUCK"
VP_PID=""

# The VP never exits on its own, so it is started in the background and killed
# once the banner shows up or TIMEOUT expires.  Writing through a process
# substitution keeps $! pointing at sep-vp itself (with a pipe it would be tee,
# and killing tee would leave the VP running).  macOS has no timeout(1), so the
# deadline is enforced here rather than by coreutils.
"$SEP_VP" "$RUN_CONFIG" "$ELF" > >(tee "$LOGFILE") 2>&1 &
VP_PID=$!

SECONDS=0
while kill -0 "$VP_PID" 2>/dev/null; do
    if grep -q "\[VP\] SIMULATION OF THE TEST PASSED" "$LOGFILE" 2>/dev/null; then
        RESULT="PASSED"; break
    fi
    if grep -q "\[VP\] SIMULATION OF THE TEST FAILED" "$LOGFILE" 2>/dev/null; then
        RESULT="FAILED"; break
    fi
    if [[ "$SECONDS" -ge "$TIMEOUT" ]]; then
        break
    fi
    sleep 0.3
done

# Stopping the VP on purpose still makes the shell announce "Terminated" for the
# job.  It reports that at the first command boundary after the signal lands, so
# the whole teardown runs with stderr closed, not just the wait.
{
    if kill -0 "$VP_PID" 2>/dev/null; then
        kill -TERM "$VP_PID" 2>/dev/null || true
        sleep 1
        kill -KILL "$VP_PID" 2>/dev/null || true
    fi
    wait "$VP_PID" || true
} 2>/dev/null

# Final check after process exits
if [[ "$RESULT" == "STUCK" ]]; then
    if grep -q "\[VP\] SIMULATION OF THE TEST PASSED" "$LOGFILE" 2>/dev/null; then
        RESULT="PASSED"
    elif grep -q "\[VP\] SIMULATION OF THE TEST FAILED" "$LOGFILE" 2>/dev/null; then
        RESULT="FAILED"
    elif ! grep -q "." "$LOGFILE" 2>/dev/null; then
        RESULT="CRASH"
    fi
fi

echo ""
echo "Log: $LOGFILE"
echo ""

case "$RESULT" in
    PASSED)
        echo "[VP] SIMULATION OF THE TEST PASSED — $TEST_NAME"
        exit 0
        ;;
    FAILED)
        echo "[VP] SIMULATION OF THE TEST FAILED — $TEST_NAME"
        exit 1
        ;;
    STUCK)
        echo "STUCK — no result after ${TIMEOUT}s — $TEST_NAME"
        exit 2
        ;;
    CRASH)
        echo "CRASH — VP produced no output — $TEST_NAME"
        exit 3
        ;;
esac
