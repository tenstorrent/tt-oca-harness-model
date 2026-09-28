#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

"""Build and run positive and key-mismatch secure-boot tests through SEP VP."""

import argparse
import queue
import subprocess
import sys
import threading
import time
from pathlib import Path


BOOTCODE_DIR = Path(__file__).resolve().parents[1]
REPO_ROOT = Path(__file__).resolve().parents[7]
CONFIG_DIR = REPO_ROOT / "vp/platform/sep/config"
SECURE_BUILD_DIR = BOOTCODE_DIR / "build/secure"


def find_vp(explicit):
    candidates = [
        Path(explicit).expanduser() if explicit else None,
        REPO_ROOT / "vp/build_sep/bin/sep-vp",
        REPO_ROOT / "vp/build/bin/sep-vp",
    ]
    for candidate in candidates:
        if candidate and candidate.is_file():
            return candidate
    raise FileNotFoundError("sep-vp not found; pass --vp or build vp/build_sep/bin/sep-vp")


def run_until_marker(vp, config, required, forbidden, timeout, log_path):
    process = subprocess.Popen(
        [str(vp), str(config)],
        cwd=REPO_ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        errors="replace",
        bufsize=1,
    )
    lines = queue.Queue()

    def read_output():
        assert process.stdout is not None
        for line in process.stdout:
            lines.put(line)
        lines.put(None)

    reader = threading.Thread(target=read_output, daemon=True)
    reader.start()
    output = ""
    deadline = time.monotonic() + timeout
    try:
        while time.monotonic() < deadline:
            try:
                line = lines.get(timeout=0.2)
            except queue.Empty:
                if process.poll() is not None:
                    break
                continue
            if line is None:
                break
            output += line
            if any(marker in output for marker in forbidden):
                raise AssertionError(
                    f"{config.name}: observed forbidden marker "
                    f"{next(marker for marker in forbidden if marker in output)}"
                )
            if all(marker in output for marker in required):
                return output
        missing = [marker for marker in required if marker not in output]
        raise AssertionError(f"{config.name}: timed out or exited; missing {missing}")
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        log_path.write_text(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vp", help="Path to sep-vp")
    parser.add_argument("--timeout", type=float, default=120.0)
    parser.add_argument("--skip-build", action="store_true")
    args = parser.parse_args()

    if not args.skip_build:
        subprocess.run(
            ["make", "secure_boot_mismatch"],
            cwd=BOOTCODE_DIR,
            check=True,
        )

    vp = find_vp(args.vp)
    SECURE_BUILD_DIR.mkdir(parents=True, exist_ok=True)

    positive = run_until_marker(
        vp,
        CONFIG_DIR / "accellera_config_secure_boot.ini",
        required=[
            "RSA_VERIFY_START",
            "SIG_VALID",
            "CRYPTO_VALIDATE_OK",
            "PLD_HASH_OK",
            "BL1_JUMP=0x10020000",
        ],
        forbidden=["PUBK_HASH_MISMATCH", "RSA_VERIFY_FAIL", "CRYPTO_FAIL="],
        timeout=args.timeout,
        log_path=SECURE_BUILD_DIR / "positive-vp.log",
    )
    print("secure boot positive: PASS", file=sys.stderr)

    negative = run_until_marker(
        vp,
        CONFIG_DIR / "accellera_config_secure_boot_mismatch.ini",
        required=["PUBK_HASH_MISMATCH", "CRYPTO_FAIL="],
        forbidden=["SIG_VALID", "SIMULATION OF THE TEST PASSED"],
        timeout=args.timeout,
        log_path=SECURE_BUILD_DIR / "mismatch-vp.log",
    )
    print("secure boot key mismatch: PASS", file=sys.stderr)
    return 0 if positive and negative else 1


if __name__ == "__main__":
    sys.exit(main())
