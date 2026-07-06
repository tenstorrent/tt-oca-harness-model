#!/usr/bin/env python3
"""Generate a small SPI-flash fixture image for the mux/flash-loader test.

Layout (mirrors the dual-bank software layout the VP boots from):
  offset 0x0000 : bank-A manifest magic  b"MAN1"  (bytes the firmware checks)
  offset 0x1000 : bank-B manifest magic  b"MAN2"
  elsewhere     : a non-0xFF pattern so staged reads are distinguishable from erased flash

The first four bytes MUST match FIX_B0..FIX_B3 in main.c.

Usage: gen_flash_fixture.py <output.bin>
"""
import sys

SIZE = 0x2000  # 8 KiB: two 4 KiB banks


def main(out_path: str) -> None:
    # Base pattern: incrementing low byte, clearly not erased (0xFF) flash.
    img = bytearray((i & 0x7F) for i in range(SIZE))
    img[0x0000:0x0004] = b"MAN1"  # bank-A manifest magic
    img[0x1000:0x1004] = b"MAN2"  # bank-B manifest magic
    with open(out_path, "wb") as f:
        f.write(img)
    print(f"[gen_flash_fixture] wrote {len(img)} bytes to {out_path}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: gen_flash_fixture.py <output.bin>")
    main(sys.argv[1])
