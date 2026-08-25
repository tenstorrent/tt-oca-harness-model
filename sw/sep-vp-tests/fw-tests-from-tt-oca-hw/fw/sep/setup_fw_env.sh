#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# Source this script before building any fw/sep or fw/smc tests:
#   source fw/setup_fw_env.sh   (from project root)
#   source setup_fw_env.sh      (from fw/ directory)
#
# Build targets:
#   make              -> RTL build  (ITCM=0xC0000000, DTCM=0xC0040000)
#   make PLATFORM=vp  -> VP  build  (ITCM=0x01000000, DTCM=0x00000000)

if [ -n "$BASH_VERSION" ]; then
    _SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
elif [ -n "$ZSH_VERSION" ]; then
    _SCRIPT_DIR="$(cd "$(dirname "${(%):-%x}")" && pwd)"
else
    echo "ERROR: Unsupported shell. Use bash or zsh." >&2
    return 1
fi

# fw/ lives one level below the project root
export OCH_ROOT="$(cd "$_SCRIPT_DIR/.." && pwd)"
export RV_ROOT="$OCH_ROOT/deps/el2"

unset _SCRIPT_DIR

echo "OCH_ROOT -> $OCH_ROOT"
echo "RV_ROOT  -> $RV_ROOT"
echo "Firmware environment ready."
