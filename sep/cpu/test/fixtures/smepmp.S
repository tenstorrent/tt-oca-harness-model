# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#
# Smepmp / mscause program for veeriss_tb (FUNC-CPU-017..). Replays the SEP
# boot ROM's PMP sequence (mseccfg.RLB, lock rules, pmpcfg read-back,
# RLB|MMWP, csrs X onto locked entries 3 and 5), then checks RLB/MMWP
# legalization and MMWP access faults. Read-backs go to SRAM+0x100, the trap
# handler logs {mcause, mscause, mtval} per trap at SRAM+0x200.
#
# Regenerate smepmp.hex after editing:
#   llvm-mc -triple=riscv32 -mattr=-c,-relax -filetype=obj smepmp.S -o smepmp.o
#   llvm-objcopy -O binary smepmp.o smepmp.bin
#   python3 -c 'import sys; b=open("smepmp.bin","rb").read(); print("@c0000000"); [print(" ".join(f"{x:02x}" for x in b[i:i+4])) for i in range(0,len(b),4)]' > smepmp.hex

    .text
    .globl _start
_start:
    li    s0, 0x10000100        # results
    li    s2, 0x10000200        # trap log
    li    t0, 0xC0000200
    csrw  mtvec, t0
    csrr  t0, 0x7ff             # mscause
    sw    t0, 0(s0)
    csrr  t0, 0x757             # mseccfgh
    sw    t0, 4(s0)
    li    t0, 4                 # RLB
    csrw  0x747, t0
    li    t0, 0x30007FFF        # 0: ICCM 0xC0000000/256K
    csrw  pmpaddr0, t0
    li    t0, 0x06001FFF        # 1: 0x18000000/64K
    csrw  pmpaddr1, t0
    li    t0, 0x04007FFF        # 3: SRAM 0x10000000/256K
    csrw  pmpaddr3, t0
    li    t0, 0x08001FFF        # 4: 0x20000000/64K
    csrw  pmpaddr4, t0
    li    t0, 0x10001FFF        # 5: 0x40000000/64K
    csrw  pmpaddr5, t0
    li    t0, 0x14001FFF        # 6: 0x50000000/64K
    csrw  pmpaddr6, t0
    li    t0, 0x9b009b9d
    csrw  pmpcfg0, t0
    csrr  t1, pmpcfg0
    sw    t1, 8(s0)
    li    t0, 0x009b9b9b
    csrw  pmpcfg1, t0
    csrr  t1, pmpcfg1
    sw    t1, 12(s0)
    li    a0, 0x60000000        # matches no entry
    lw    t1, 0(a0)             # MMWP=0: allowed
    li    t0, 6                 # RLB | MMWP
    csrw  0x747, t0
    csrr  t1, 0x747
    sw    t1, 16(s0)
    li    t0, 0x04000000        # X onto locked entry 3
    csrs  pmpcfg0, t0
    csrr  t1, pmpcfg0
    sw    t1, 20(s0)
    li    t0, 0x00000400        # X onto locked entry 5
    csrs  pmpcfg1, t0
    csrr  t1, pmpcfg1
    sw    t1, 24(s0)
    li    t0, 0x06005FFF        # locked pmpaddr1, RLB=1
    csrw  pmpaddr1, t0
    csrr  t1, pmpaddr1
    sw    t1, 28(s0)
    li    t0, 4
    csrc  0x747, t0             # clear RLB
    csrr  t1, 0x747
    sw    t1, 32(s0)
    csrs  0x747, t0             # RLB cannot be set again
    csrr  t1, 0x747
    sw    t1, 36(s0)
    li    t0, 3
    csrc  0x747, t0             # MML/MMWP sticky
    csrr  t1, 0x747
    sw    t1, 40(s0)
    li    t0, 0x04000000        # locked entry 3, RLB=0
    csrc  pmpcfg0, t0
    csrr  t1, pmpcfg0
    sw    t1, 44(s0)
    li    t0, 0x06009FFF        # locked pmpaddr1, RLB=0
    csrw  pmpaddr1, t0
    csrr  t1, pmpaddr1
    sw    t1, 48(s0)
    auipc s1, 0
    addi  s1, s1, 12
    lw    t1, 0(a0)             # MMWP: load access fault
    auipc s1, 0
    addi  s1, s1, 12
    sw    t1, 0(a0)             # MMWP: store access fault
    auipc s1, 0
    addi  s1, s1, 12
    jalr  x0, 0(a0)             # MMWP: fetch access fault
    li    a1, 0x20000000        # matches entry 4
    lw    t1, 0(a1)
    sw    t1, 0(a1)
    sw    s2, 52(s0)
    li    t0, 0x600d
    sw    t0, 56(s0)
1:  j     1b

    .org  0x200
trap:
    csrr  t0, mcause
    sw    t0, 0(s2)
    csrr  t0, 0x7ff
    sw    t0, 4(s2)
    csrr  t0, mtval
    sw    t0, 8(s2)
    addi  s2, s2, 12
    csrw  mepc, s1
    mret
