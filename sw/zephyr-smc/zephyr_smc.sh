#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Setup / build / run Zephyr on the SMC virtual platform.
# See doc/zephyr-on-smc.adoc.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
WS="${ZEPHYR_WORKSPACE:-$HERE/.workspace}"
ZEPHYR_VERSION="${ZEPHYR_VERSION:-v4.3.0}"
BOARD="${BOARD:-smc_vp}"

die() { echo "error: $*" >&2; exit 1; }
log() { echo "==> $*"; }

find_west() {
    if [[ -x "$HERE/.venv/bin/west" ]]; then
        echo "$HERE/.venv/bin/west"
        return
    fi
    if command -v west >/dev/null 2>&1; then
        command -v west
        return
    fi
    for d in "$HOME/Library/Python/3.13/bin" "$HOME/Library/Python/3.12/bin" \
             "$HOME/Library/Python/3.11/bin" "$HOME/Library/Python/3.9/bin" \
             "$HOME/.local/bin"; do
        if [[ -x "$d/west" ]]; then
            echo "$d/west"
            return
        fi
    done
    return 1
}

find_cross_prefix() {
    if [[ -n "${CROSS_COMPILE:-}" ]]; then
        echo "$CROSS_COMPILE"
        return
    fi
    local p
    for p in "${RISCV_PREFIX:-}" riscv64-elf- riscv64-unknown-elf- riscv-none-elf-; do
        [[ -z "$p" ]] && continue
        if command -v "${p}gcc" >/dev/null 2>&1; then
            local gcc
            gcc="$(command -v "${p}gcc")"
            echo "$(dirname "$gcc")/${p}"
            return
        fi
    done
    die "no RISC-V gcc on PATH (tried riscv64-elf-, riscv64-unknown-elf-, riscv-none-elf-). Set CROSS_COMPILE or RISCV_PREFIX."
}

find_smc_vp() {
    if [[ -n "${SMC_VP:-}" && -x "$SMC_VP" ]]; then
        echo "$SMC_VP"
        return
    fi
    local c
    for c in "$REPO/vp/build_smc/bin/smc-vp" "$REPO/vp/build/bin/smc-vp"; do
        if [[ -x "$c" ]]; then
            echo "$c"
            return
        fi
    done
    return 1
}

ensure_smc_vp() {
    if find_smc_vp >/dev/null; then
        find_smc_vp
        return
    fi
    log "smc-vp not found; building vp/build"
    [[ -n "${SYSTEMC_HOME:-}" ]] || die "SYSTEMC_HOME is not set"
    [[ -n "${CCI_HOME:-}" ]] || die "CCI_HOME is not set"
    cmake -S "$REPO/vp" -B "$REPO/vp/build" -DCMAKE_BUILD_TYPE=Release \
          -DSYSTEMC_HOME="$SYSTEMC_HOME" -DCCI_HOME="$CCI_HOME"
    cmake --build "$REPO/vp/build" --target smc-vp -j
    find_smc_vp || die "smc-vp build succeeded but binary is missing"
}

cmd_setup() {
    if [[ ! -x "$HERE/.venv/bin/west" ]]; then
        local py=""
        for c in python3.12 python3.13 python3.11 python3; do
            if command -v "$c" >/dev/null 2>&1; then
                local ver
                ver="$("$c" -c 'import sys; print(f"{sys.version_info[0]}.{sys.version_info[1]}")')"
                if [[ "$(printf '%s\n' "$ver" "3.10" | sort -V | head -1)" == "3.10" ]]; then
                    py="$(command -v "$c")"
                    break
                fi
            fi
        done
        [[ -n "$py" ]] || die "Python >= 3.10 is required (Zephyr 4.3). Install python3.12."
        log "creating venv with $py"
        "$py" -m venv "$HERE/.venv"
        "$HERE/.venv/bin/pip" install -q west
    fi
    local west
    west="$(find_west)" || die "west not found after venv setup"
    if [[ -d "$WS/.west" ]]; then
        log "west workspace already exists at $WS"
    else
        log "west init Zephyr $ZEPHYR_VERSION -> $WS"
        mkdir -p "$(dirname "$WS")"
        "$west" init -m https://github.com/zephyrproject-rtos/zephyr --mr "$ZEPHYR_VERSION" "$WS"
    fi
    if [[ "${ZEPHYR_SKIP_UPDATE:-}" == "1" && -d "$WS/zephyr" ]]; then
        log "ZEPHYR_SKIP_UPDATE=1; skipping west update"
    else
        log "west update (this can take several minutes)"
        (cd "$WS" && "$west" update)
    fi
    (cd "$WS" && "$west" zephyr-export) || true
    if [[ -f "$WS/zephyr/scripts/requirements-base.txt" && -x "$HERE/.venv/bin/pip" ]]; then
        log "installing Zephyr Python requirements into .venv"
        "$HERE/.venv/bin/pip" install -q -r "$WS/zephyr/scripts/requirements-base.txt"
    fi
    log "workspace ready: $WS"
    echo "ZEPHYR_BASE=$WS/zephyr"
}

zephyr_env() {
    [[ -d "$WS/zephyr" ]] || die "Zephyr not set up. Run: $0 setup"
    export ZEPHYR_BASE="$WS/zephyr"
    export ZEPHYR_EXTRA_MODULES="$HERE"
    export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile
    export CROSS_COMPILE
    CROSS_COMPILE="$(find_cross_prefix)"
    local west
    west="$(find_west)" || die "west not found; run $0 setup"
    WEST="$west"
}

app_paths() {
    local kind="${1:-hello}"
    case "$kind" in
        hello|hello_world)
            APP="$ZEPHYR_BASE/samples/hello_world"
            BDIR="$HERE/build/hello_world"
            ;;
        shell|shell_module)
            APP="$ZEPHYR_BASE/samples/subsys/shell/shell_module"
            BDIR="$HERE/build/shell"
            ;;
        poke|mmio_poke)
            APP="$HERE/apps/mmio_poke"
            BDIR="$HERE/build/mmio_poke"
            ;;
        *)
            die "unknown app '$kind' (hello|shell|poke)"
            ;;
    esac
}

cmd_build() {
    zephyr_env
    app_paths "${1:-hello}"
    log "west build -b $BOARD $APP"
    log "CROSS_COMPILE=$CROSS_COMPILE"
    if [[ "${1:-hello}" == shell || "${1:-hello}" == shell_module ]]; then
        "$WEST" build -p auto -b "$BOARD" -d "$BDIR" "$APP" \
            -- -DEXTRA_CONF_FILE="$HERE/shell.conf"
    else
        "$WEST" build -p auto -b "$BOARD" -d "$BDIR" "$APP"
    fi
    log "ELF $BDIR/zephyr/zephyr.elf"
}

cmd_run() {
    zephyr_env
    local kind="${1:-hello}"
    app_paths "$kind"
    local elf="$BDIR/zephyr/zephyr.elf"
    [[ -f "$elf" ]] || die "no ELF at $elf — run: $0 build $kind"
    local vp
    vp="$(ensure_smc_vp)"
    local ini="$HERE/config/smc_zephyr.ini"
    case "$kind" in
        shell|shell_module)
            log "interactive UART0 — type at the uart:~$ prompt, Ctrl-C to stop"
            exec "$vp" "$ini" "$elf" 0 --uart-interactive
            ;;
        poke|mmio_poke)
            local ms="${SIM_TIME_MS:-1500}"
            log "live UART0 for ${ms} ms of simulation (mmio_poke)"
            exec "$vp" "$ini" "$elf" "$ms" --uart-live
            ;;
        *)
            local ms="${SIM_TIME_MS:-500}"
            log "live UART0 for ${ms} ms of simulation"
            exec "$vp" "$ini" "$elf" "$ms" --uart-live
            ;;
    esac
}

cmd_test() {
    local kind="${1:-poke}"
    local ms pass_pat
    case "$kind" in
        hello|hello_world)
            ms="${SIM_TIME_MS:-500}"
            pass_pat="Hello World"
            ;;
        poke|mmio_poke)
            ms="${SIM_TIME_MS:-1500}"
            pass_pat="RESULT: PASS"
            ;;
        *)
            die "test supports hello|poke (shell is interactive)"
            ;;
    esac
    cmd_build "$kind"
    app_paths "$kind"
    local elf="$BDIR/zephyr/zephyr.elf"
    local vp
    vp="$(ensure_smc_vp)"
    local ini="$HERE/config/smc_zephyr.ini"
    local log="$BDIR/run.log"
    log "smc-vp $kind (${ms} ms) -> $log"
    "$vp" "$ini" "$elf" "$ms" --uart-live | tee "$log"
    if grep -q "$pass_pat" "$log"; then
        log "$kind PASS"
    else
        die "$kind did not print '$pass_pat' (see $log)"
    fi
}

cmd_ci() {
    cmd_setup
    cmd_test hello
    cmd_test poke
}

usage() {
    cat <<EOF
Usage: $0 <setup|build|run|test|ci|help> [hello|shell|poke]

  setup            Clone Zephyr $ZEPHYR_VERSION into $WS and west update
  build [hello]    Build samples/hello_world for $BOARD
  build shell      Build samples/subsys/shell/shell_module
  build poke       Build apps/mmio_poke (unlisted-IP MMIO reachability)
  run [hello]      Boot hello_world on smc-vp with live UART0
  run shell        Boot the shell; type at the kernel (Ctrl-C to exit)
  run poke         Boot mmio_poke and stream UART0
  test [poke]      Build + run; poke requires RESULT: PASS, hello requires Hello World
  ci               setup + test hello + test poke (used by GitHub Actions)

Environment:
  ZEPHYR_WORKSPACE   west workspace (default: $HERE/.workspace)
  ZEPHYR_VERSION     git tag for west init (default: $ZEPHYR_VERSION)
  ZEPHYR_SKIP_UPDATE skip west update when the workspace already exists
  CROSS_COMPILE / RISCV_PREFIX
  SMC_VP             path to smc-vp (else vp/build_smc/bin/smc-vp)
  SIM_TIME_MS        hello_world window (default: 500; poke default: 1500)
  SYSTEMC_HOME CCI_HOME WHISPER_HOME BOOST_DIR
                     required only if smc-vp must be built
EOF
}

case "${1:-help}" in
    setup) shift; cmd_setup "$@" ;;
    build) shift; cmd_build "${1:-hello}" ;;
    run)   shift; cmd_run "${1:-hello}" ;;
    test)  shift; cmd_test "${1:-poke}" ;;
    ci)    shift; cmd_ci "$@" ;;
    help|-h|--help) usage ;;
    *) usage; exit 2 ;;
esac
