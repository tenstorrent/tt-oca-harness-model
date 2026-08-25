#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
"""Generate the SPI-flash fixture for the SPI -> secure-DMA streaming integration test.

The image is a simple position-dependent oracle:

    byte[i] = i & 0xFF

so a correct DMA read of `len` bytes from flash offset `off` must land
(off + k) & 0xFF at destination byte k. main.c reads several 512-byte regions from
distinct offsets and checks every byte against this rule, which validates addressing,
ordering, and completeness at once (a dropped/undriven read shows up as 0xFF, a
mis-drained chunk as a value/position mismatch).

Size covers every offset main.c reads (max 0x6A0 + 512 = 0x8A0); 8 KiB is comfortable.

Usage: gen_flash_fixture.py <output.bin>
"""
import sys

SIZE = 0x2000  # 8 KiB


def main(out_path: str) -> None:
    img = bytearray(i & 0xFF for i in range(SIZE))
    with open(out_path, "wb") as f:
        f.write(img)
    print(f"[gen_flash_fixture] wrote {len(img)} bytes to {out_path} (byte[i] = i & 0xFF)")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: gen_flash_fixture.py <output.bin>")
    main(sys.argv[1])
