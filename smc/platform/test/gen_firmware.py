#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# ===========================================================================
# smc/platform/test/gen_firmware.py
#
# Generates a tiny bare-metal RV64 firmware image for the smc_platform Phase-2
# test, in the bootrom "hex" preload format (one 64-bit big-endian ASCII word
# per line; the bootrom stores each parsed word host-endian, so the CPU fetches
# little-endian instruction bytes).
#
# The firmware:
#   1. writes 'U' (0x55) to UART[0] THR  (0xC000'A000)
#   2. writes 0x1234'ABCD to scratchpad RAM word 0 (0xC006'0000)
#   3. writes 'I' (0x49) to UART[0] THR
#   4. busy-loops forever (jal x0, 0)
#
# The test bench asserts: scratchpad[0] == 0x1234'ABCD and the UART TX history
# starts with "UI".
# ===========================================================================

import struct
import sys


def lui(rd: int, imm20: int) -> int:
    imm20 &= 0xFFFFF
    return (imm20 << 12) | (rd << 7) | 0x37


def addi(rd: int, rs1: int, imm12: int) -> int:
    imm12 &= 0xFFF
    return (imm12 << 20) | (rs1 << 15) | (0 << 12) | (rd << 7) | 0x13


def sb(rs2: int, rs1: int, imm12: int) -> int:
    imm12 &= 0xFFF
    imm_hi = (imm12 >> 5) & 0x7F   # imm[11:5] -> bits[31:25]
    imm_lo = imm12 & 0x1F          # imm[4:0]   -> bits[11:7]
    return (imm_hi << 25) | (rs2 << 20) | (rs1 << 15) | (0 << 12) | (imm_lo << 7) | 0x23


def sw(rs2: int, rs1: int, imm12: int) -> int:
    imm12 &= 0xFFF
    imm_hi = (imm12 >> 5) & 0x7F
    imm_lo = imm12 & 0x1F
    return (imm_hi << 25) | (rs2 << 20) | (rs1 << 15) | (2 << 12) | (imm_lo << 7) | 0x23


def jal(rd: int, imm21: int) -> int:
    # imm21 is the signed byte offset (multiple of 2).
    imm = imm21 & 0x1FFFFF
    b20 = (imm >> 20) & 0x1
    b10_1 = (imm >> 1) & 0x3FF
    b11 = (imm >> 11) & 0x1
    b19_12 = (imm >> 12) & 0xFF
    enc = (b20 << 31) | (b10_1 << 21) | (b11 << 20) | (b19_12 << 12) | (rd << 7) | 0x6F
    return enc & 0xFFFFFFFF


def slli(rd: int, rs1: int, shamt: int) -> int:
    # RV64: shamt is 6 bits (0..63). funct3=1 (SLLI), funct6=0.
    shamt &= 0x3F
    return (shamt << 20) | (rs1 << 15) | (1 << 12) | (rd << 7) | 0x13


def srli(rd: int, rs1: int, shamt: int) -> int:
    # RV64: shamt is 6 bits. funct3=5 (SRLI), funct6=0.
    shamt &= 0x3F
    return (shamt << 20) | (rs1 << 15) | (5 << 12) | (rd << 7) | 0x13


def load_addr64(rd: int, addr: int) -> list:
    # Load a 64-bit address into rd. lui sign-extends bit 31 in RV64, so for
    # addresses >= 0x80000000 we clear the high 32 bits via slli/srli by 32.
    # addi sign-extends its 12-bit immediate, so when lo12 >= 0x800 we must
    # increment hi20 by 1 to compensate.
    hi20 = (addr >> 12) & 0xFFFFF
    lo12 = addr & 0xFFF
    if lo12 >= 0x800:
        hi20 = (hi20 + 1) & 0xFFFFF
        imm12 = (lo12 - 0x1000) & 0xFFF  # negative
    else:
        imm12 = lo12
    insns = [lui(rd, hi20)]
    if lo12:
        insns.append(addi(rd, rd, imm12))
    if addr & 0x80000000:
        # bit 31 set -> lui sign-extended the high 32 bits to 0xffffffff.
        # slli 32 then srli 32 zero-extends the low 32 bits.
        insns.append(slli(rd, rd, 32))
        insns.append(srli(rd, rd, 32))
    return insns


def write_hex(insns: list, out_path: str) -> None:
    # Pad to an even instruction count so we can pack two 32-bit insns per
    # 64-bit bootrom word.
    if len(insns) % 2 != 0:
        insns.append(jal(0, 0))  # nop-ish busy loop filler
    with open(out_path, "w") as f:
        for i in range(0, len(insns), 2):
            lo = insns[i] & 0xFFFFFFFF
            hi = insns[i + 1] & 0xFFFFFFFF
            # bootrom hex: a 64-bit value per line, big-endian ASCII.  The
            # bootrom memcpy's the parsed uint64_t into data_ (host-endian =
            # little-endian here), so the low 32 bits become the first
            # instruction's little-endian bytes.
            word = lo | (hi << 32)
            f.write(f"{word:016x}\n")


def build_firmware() -> list:
    # Register aliases
    X1, X2, X3, X4, X5 = 1, 2, 3, 4, 5

    UART0    = 0xC000A000   # UART0 base
    OFF_THR  = 0x00         # RBR/THR (DLAB=0) / DLL (DLAB=1)
    OFF_IER  = 0x04         # IER      (DLAB=0) / DLM (DLAB=1)
    OFF_LCR  = 0x0C         # LCR (bit 7 = DLAB)
    SCRATCH  = 0xC0060000   # scratchpad RAM word 0

    insns = []
    # --- UART init: divisor must be non-zero for tx_enabled() to return true.
    insns += load_addr64(X1, UART0)            # x1 = UART0 base
    insns.append(addi(X3, 0, 0x080))            # x3 = 0x80 (DLAB=1)
    insns.append(sw(X3, X1, OFF_LCR))           # LCR = 0x80
    insns.append(addi(X3, 0, 0x001))            # x3 = 1
    insns.append(sw(X3, X1, OFF_THR))           # DLL = 1
    insns.append(addi(X3, 0, 0x000))            # x3 = 0
    insns.append(sw(X3, X1, OFF_IER))           # DLM = 0  (divisor = 1)
    insns.append(addi(X3, 0, 0x003))            # x3 = 0x03 (8N1, DLAB=0)
    insns.append(sw(X3, X1, OFF_LCR))           # LCR = 0x03

    # --- Emit 'U' on UART0.
    insns.append(addi(X3, 0, 0x055))            # x3 = 'U'
    insns.append(sw(X3, X1, OFF_THR))           # THR = 'U'

    # --- Write 0x1234'ABCD to scratchpad word 0.
    insns += load_addr64(X2, SCRATCH)
    insns += load_addr64(X4, 0x1234ABCD)
    insns.append(sw(X4, X2, 0))                 # scratchpad[0] = 0x1234ABCD

    # --- Emit 'I' on UART0.
    insns.append(addi(X5, 0, 0x049))            # x5 = 'I'
    insns.append(sw(X5, X1, OFF_THR))           # THR = 'I'

    # --- Busy loop.
    insns.append(jal(0, 0))
    return insns


def build_park() -> list:
    # Park firmware: set mtvec to the bootrom base (so any trap returns to a
    # `jal x0,0` spin instead of vectoring to mtvec=0 / unmapped memory and
    # looping on illegal instructions), then spin.  One store to an address
    # outside the cluster's MMIO aperture routes via cluster.data to the
    # platform's multi_stub_target (exercising its b_transport); it is RAZ/WI
    # so it has no side effect.  The rest of the image is filled with `jal x0,0`
    # so a hart that lands anywhere in the loaded region loops harmlessly.
    BOOTROM = 0xC0040000
    OUT_OF_MMIO = 0xC2000000   # outside [0xC0000000, 0xC1000000) -> data socket
    # csrw mtvec, x1  (csrrw x0, mtvec=0x305, x1) -> 0x30501073
    csrw_mtvec_x1 = (0x305 << 20) | (1 << 15) | (1 << 12) | (0 << 7) | 0x73
    insns = []
    insns += load_addr64(1, BOOTROM)            # x1 = bootrom base
    insns.append(csrw_mtvec_x1)                  # mtvec = bootrom base (Direct)
    insns += load_addr64(2, OUT_OF_MMIO)        # x2 = out-of-mmio address
    insns.append(addi(3, 0, 0x1))                # x3 = 1
    insns.append(sw(3, 2, 0))                    # store -> cluster.data -> stub_data
    while len(insns) < 128:                      # 512 bytes of image
        insns.append(jal(0, 0))
    return insns


def main() -> int:
    args = sys.argv[1:]
    park = "--park" in args
    args = [a for a in args if a != "--park"]
    out_path = args[0] if args else ("park.hex" if park else "firmware.hex")

    insns = build_park() if park else build_firmware()
    write_hex(insns, out_path)

    # Also emit a disassembly summary to stderr for debugging.
    for idx, enc in enumerate(insns):
        sys.stderr.write(f"0x{idx * 4:04x}: {enc & 0xFFFFFFFF:08x}\n")
    sys.stderr.write(f"=> wrote {out_path}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
