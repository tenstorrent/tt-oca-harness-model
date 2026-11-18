// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// Minimal BL1 test payload for OROM boot flow verification (C version).
//
// After the Boot ROM loads this image into ICCM (.text) and DCCM (.data),
// it jumps to _start, which prints debug messages via virtual console,
// configures OBF, then signals PASS via STDOUT mailbox.
//
// Virtual console output (appears as text in cocotb sim log):
//   "BL1\n"     — BL1 entry (proves jump to ICCM succeeded)
//   "OBF\n"     — Outbound filter configured
//   "GO!\n"     — About to write PASS to STDOUT mailbox
//
// Memory layout (VeeR EL2 constraint):
//   .text   → ICCM (0xC0000000) — IFU fetches instructions here
//   .rodata → DCCM (0xC0040000) — LSU reads data here
//   .bss    → DCCM              — LSU reads/writes here
//
// Self-contained: no external headers, no crt0 needed.
// BL1 inherits CPU state (SP, PMA, mtvec) from the ROM.

#include <stdint.h>

// ---------------------------------------------------------------------------
// BSS zeroing (DCCM is uninitialized RAM)
// ---------------------------------------------------------------------------
extern uint32_t BSS_START;
extern uint32_t BSS_END;

// ---------------------------------------------------------------------------
// MMIO helper
// ---------------------------------------------------------------------------
static inline void mmio_write32(uint32_t addr, uint32_t val)
{
    *(volatile uint32_t *)(uintptr_t)addr = val;
}

static inline uint32_t mmio_read32(uint32_t addr)
{
    return *(volatile uint32_t *)(uintptr_t)addr;
}

// ---------------------------------------------------------------------------
// Scratch register virtual console (same protocol as rom_virt_console.h)
// ---------------------------------------------------------------------------
#define SCRATCH2_ADDR 0x10802010u

#define VCONSOLE_OP_ASCII (0u << 1)

static uint32_t g_vconsole_prev;

static inline void vc_write(uint32_t val)
{
    if (val == g_vconsole_prev)
        val ^= 1u;
    mmio_write32(SCRATCH2_ADDR, val);
    g_vconsole_prev = val;
}

static inline void bl1_puts(const char *s)
{
    uint32_t val = VCONSOLE_OP_ASCII;
    int off = 1;
    while (*s)
    {
        val |= ((uint32_t)(uint8_t)*s++) << (8u * (uint32_t)off++);
        if (off == 4)
        {
            vc_write(val);
            off = 1;
            val = VCONSOLE_OP_ASCII;
        }
    }
    if (off != 1)
        vc_write(val);
}

// ---------------------------------------------------------------------------
// Hex print helper for debug output
// ---------------------------------------------------------------------------
static void bl1_puthex32(uint32_t val)
{
    // Avoid static const array — -fdata-sections puts it in .rodata.xxx
    // which may not be included in the data binary.
    char buf[11]; // "0x" + 8 hex digits + '\0'
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 7; i >= 0; i--)
    {
        uint8_t nib = (uint8_t)((val >> (4u * (uint32_t)i)) & 0xFu);
        buf[2 + (7 - i)] = (char)(nib < 10u ? '0' + nib : 'A' + nib - 10u);
    }
    buf[10] = '\0';
    bl1_puts(buf);
}

// ---------------------------------------------------------------------------
// Fuse read-lock verification
//
// The ROM read-locks the secret fuses before the BL1 handoff. For the class key
// and both RMA token digests, BL1 checks that the lock bits are set and that a
// read of each locked field completes with a bus error. On VeeR-EL2 a D-bus load
// error is an imprecise NMI with mcause 0xF0000001 and the load address latched
// in mdseac; the core drops the load data.
// ---------------------------------------------------------------------------
#define EFUSE_LOCKS_ADDR 0x10930000u

// Read-lock bit positions in the LOCKS register (same as ROM's fuse_lock.h):
//   CLASS_KEY_READ_LOCK       = bit 15
//   RMA_SOP_TOKEN_READ_LOCK   = bit 13
//   RMA_CHIPLET_TOKEN_READ_LOCK = bit 11
#define FUSE_SECRET_READ_LOCK_MASK 0x0000A800u

// Locked field addresses for direct read verification
#define CLASS_KEY_ADDR            0x10930064u
#define RMA_SIP_TOKEN_ADDR        0x10930024u
#define RMA_CHIPLET_TOKEN_ADDR    0x10930044u

#define NMI_VEC_ADDR              0x10A30180u
#define NMI_VEC_LOCK_ADDR         0x10A30188u
#define MCAUSE_NMI_LOAD_BUS_ERROR 0xF0000001u
#define CSR_MDSEAC                0xFC0
#define CSR_MDEAU                 0xBC0

// Bounds the wait for the imprecise NMI after a locked read; the NMI ends it.
#define NMI_WAIT_LIMIT 1000u

// Written by the NMI handler. An NMI counts toward a read only while that read
// has armed g_nmi_armed; any other NMI is stray.
static volatile uint32_t g_nmi_armed;
static volatile uint32_t g_nmi_count;
static volatile uint32_t g_nmi_mcause;
static volatile uint32_t g_nmi_mdseac;
static volatile uint32_t g_nmi_stray;
static volatile uint32_t g_nmi_stray_mcause;

// Records the NMI and returns to the interrupted code. Writing mdeau unlocks
// mdseac; until then the core raises no NMI for a later D-bus error.
__attribute__((interrupt("machine"), aligned(4), used)) static void bl1_nmi_handler(void)
{
    uint32_t mcause, mdseac;
    __asm__ volatile("csrr %0, mcause" : "=r"(mcause));
    __asm__ volatile("csrr %0, %1" : "=r"(mdseac) : "i"(CSR_MDSEAC));
    __asm__ volatile("csrw %0, zero" ::"i"(CSR_MDEAU));
    if (!g_nmi_armed) {
        g_nmi_stray++;
        g_nmi_stray_mcause = mcause;
        return;
    }
    g_nmi_count++;
    g_nmi_mcause = mcause;
    g_nmi_mdseac = mdseac;
}

// The vector takes the handler's run address, wherever the ROM placed BL1.
static int bl1_nmi_init(void)
{
    uint32_t want;
    __asm__ volatile("lla %0, bl1_nmi_handler" : "=r"(want));

    mmio_write32(NMI_VEC_ADDR, want);
    uint32_t got = mmio_read32(NMI_VEC_ADDR);
    bl1_puts("NMI_VEC=");
    bl1_puthex32(got);
    bl1_puts("\n");
    if (got != want) {
        bl1_puts("FAIL:NMI_VEC=");
        bl1_puthex32(got);
        bl1_puts(" EXPECTED=");
        bl1_puthex32(want);
        bl1_puts("\n");
        return 1;
    }
    mmio_write32(NMI_VEC_LOCK_ADDR, 1u);
    return 0;
}

// Returns 0 if all secret fuse read-lock bits are set, nonzero on failure.
static int bl1_verify_fuse_locks(void)
{
    uint32_t locks = mmio_read32(EFUSE_LOCKS_ADDR);

    bl1_puts("LOCKS=");
    bl1_puthex32(locks);
    bl1_puts("\n");

    if ((locks & FUSE_SECRET_READ_LOCK_MASK) != FUSE_SECRET_READ_LOCK_MASK) {
        bl1_puts("FAIL:LOCKS\n");
        bl1_puts("EXPECTED=");
        bl1_puthex32(FUSE_SECRET_READ_LOCK_MASK);
        bl1_puts("\n");
        return 1;
    }

    return 0;
}

// Reads addr with the NMI expectation armed. The returned value is meaningful
// only when no NMI arrived, since a faulting load does not write its register.
static uint32_t bl1_bracketed_read(uint32_t addr)
{
    g_nmi_count = 0;
    g_nmi_mcause = 0;
    g_nmi_mdseac = 0;
    g_nmi_armed = 1;
    uint32_t val = mmio_read32(addr);
    for (uint32_t i = 0; i < NMI_WAIT_LIMIT && g_nmi_count == 0; i++) {
    }
    g_nmi_armed = 0;
    return val;
}

// Prints the NMIs a bracketed read of `field` took. Returns 0 if there was
// exactly one, a load bus error at addr.
static int bl1_grade_locked_read_nmi(const char *field, uint32_t addr)
{
    uint32_t n = g_nmi_count;
    uint32_t mcause = g_nmi_mcause;
    uint32_t mdseac = g_nmi_mdseac;

    bl1_puts("LOCK_RD_NMI=");
    bl1_puts(field);
    bl1_puts(" N=");
    bl1_puthex32(n);
    bl1_puts(" MC=");
    bl1_puthex32(mcause);
    bl1_puts(" DSEAC=");
    bl1_puthex32(mdseac);
    bl1_puts("\n");

    if (n != 1u) {
        bl1_puts("FAIL:LOCK_RD_NMI_COUNT=");
        bl1_puthex32(n);
        bl1_puts("\n");
        return 1;
    }
    if (mcause != MCAUSE_NMI_LOAD_BUS_ERROR) {
        bl1_puts("FAIL:LOCK_RD_MCAUSE=");
        bl1_puthex32(mcause);
        bl1_puts(" EXPECTED=");
        bl1_puthex32(MCAUSE_NMI_LOAD_BUS_ERROR);
        bl1_puts("\n");
        return 1;
    }
    if (mdseac != addr) {
        bl1_puts("FAIL:LOCK_RD_DSEAC=");
        bl1_puthex32(mdseac);
        bl1_puts(" EXPECTED=");
        bl1_puthex32(addr);
        bl1_puts("\n");
        return 1;
    }
    return 0;
}

// Returns 0 if no NMI has arrived outside a locked-field read.
static int bl1_check_stray_nmi(void)
{
    if (g_nmi_stray == 0u)
        return 0;
    bl1_puts("FAIL:NMI_STRAY=");
    bl1_puthex32(g_nmi_stray);
    bl1_puts(" MC=");
    bl1_puthex32(g_nmi_stray_mcause);
    bl1_puts("\n");
    return 1;
}

// Returns 0 if every read-locked field faults with its own address and no NMI
// arrives outside a read. A read that takes no NMI prints the value it returned.
static int bl1_test_locked_field_reads(void)
{
    static const struct {
        const char *name;
        uint32_t addr;
    } fields[] = {
        {"CLASS_KEY", CLASS_KEY_ADDR},
        {"RMA_SIP_TOKEN_DIGEST", RMA_SIP_TOKEN_ADDR},
        {"RMA_CHIPLET_TOKEN_DIGEST", RMA_CHIPLET_TOKEN_ADDR},
    };

    for (unsigned i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        uint32_t val = bl1_bracketed_read(fields[i].addr);
        if (g_nmi_count == 0u) {
            bl1_puts("FAIL:");
            bl1_puts(fields[i].name);
            bl1_puts("=");
            bl1_puthex32(val);
            bl1_puts("\n");
            return 1;
        }
        if (bl1_grade_locked_read_nmi(fields[i].name, fields[i].addr))
            return 1;
    }

    return bl1_check_stray_nmi();
}

// ---------------------------------------------------------------------------
// Outbound filter configuration (self-contained, hardcoded addresses)
// ---------------------------------------------------------------------------
#define OBF_BASE 0x10A20000u
#define OBF_CONFIG (OBF_BASE + 0x00u)
#define OBF_START_ADDR (OBF_BASE + 0x08u)
#define OBF_END_ADDR (OBF_BASE + 0x10u)

static inline void bl1_outbound_filter_init(void)
{
    // START_ADDR = 0x0000000080000000
    mmio_write32(OBF_START_ADDR, 0x80000000u);
    mmio_write32(OBF_START_ADDR + 4, 0x00000000u);

    // END_ADDR = 0x00000000800000FF
    mmio_write32(OBF_END_ADDR, 0x800000FFu);
    mmio_write32(OBF_END_ADDR + 4, 0x00000000u);

    __asm__ volatile("fence w, w" ::: "memory");

    // CONFIG = 0x0000000101000013
    mmio_write32(OBF_CONFIG, 0x01000013u);
    mmio_write32(OBF_CONFIG + 4, 0x00000001u);

    __asm__ volatile("fence w, w" ::: "memory");
}

// ---------------------------------------------------------------------------
// STDOUT mailbox — PASS/FAIL magic (testbench protocol)
// ---------------------------------------------------------------------------
#define STDOUT_ADDR 0x80000000u
#define FW_MAGIC0 0xA5A55A5Au
#define FW_PASS 0xCAFEBABEu

static inline void mbx_putw(uint32_t w)
{
    mmio_write32(STDOUT_ADDR, w);
}

// ---------------------------------------------------------------------------
// Entry point — called directly by ROM's jump_to_bl1().
// ---------------------------------------------------------------------------
__attribute__((section(".text.init"))) void _start(void)
{
    // Zero BSS in DCCM (uninitialized RAM).
    for (uint32_t *p = &BSS_START; p < &BSS_END; p++)
        *p = 0;

    bl1_puts("BL1\n");

    // Every NMI from here on reaches bl1_nmi_handler, which needs .bss cleared.
    if (bl1_nmi_init())
    {
        mbx_putw(FW_MAGIC0);
        mbx_putw(0xDEADDEADu);
        for (;;)
            __asm__ volatile("wfi");
    }

    bl1_outbound_filter_init();

    bl1_puts("OBF\n");

    // Verify ROM's fuse read-locks are effective: LOCKS register bits must be set.
    bl1_puts("FUSE_CHK\n");
    int fuse_fail = bl1_verify_fuse_locks();
    if (fuse_fail)
    {
        bl1_puts("FUSE_LOCK_VERIFY_FAIL\n");
        mbx_putw(FW_MAGIC0);
        mbx_putw(0xDEADDEADu); // FAIL
        for (;;)
            __asm__ volatile("wfi");
    }
    bl1_puts("FUSE_OK\n");

    bl1_puts("LOCK_RD\n");
    int read_fail = bl1_test_locked_field_reads();
    if (read_fail) {
        bl1_puts("LOCK_RD_FAIL\n");
        mbx_putw(FW_MAGIC0);
        mbx_putw(0xDEADDEADu);
        for (;;)
            __asm__ volatile("wfi");
    }
    bl1_puts("LOCK_RD_OK\n");

    // Catches an NMI that arrives after the locked-read check returns.
    if (bl1_check_stray_nmi()) {
        mbx_putw(FW_MAGIC0);
        mbx_putw(0xDEADDEADu);
        for (;;)
            __asm__ volatile("wfi");
    }

    bl1_puts("GO!\n");

    mbx_putw(FW_MAGIC0);
    mbx_putw(FW_PASS);

    // Park: spin in WFI loop.
    for (;;)
    {
        __asm__ volatile("wfi");
    }
}
