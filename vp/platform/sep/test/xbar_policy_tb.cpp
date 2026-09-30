// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// ===========================================================================
// vp/platform/sep/test/xbar_policy_tb.cpp
//
// Unit test for vp/platform/sep/inc/xbar_policy.h — the SEP local AXI
// crossbar connectivity matrix transcribed from
// tt-oca-hw meta/crossbars/configs/sep_local_axi_xbar.yaml.
//
// The header is plain C++ (no SystemC), so this is a host-only executable.
// Every expected value below is taken from the yaml / sep_local_axi_xbar.sv
// address rules, not from the header, so a transcription error in either
// direction fails the test.
//
// Sections
//   1. classify(): first / last / one-past-last byte of every subordinate
//      window, including the csr_region trim from tt-oca-hw #2587
//      (0x10A00000..0x10A4FFFF is sep_system_peripherals; 0x10A50000..
//      0x10A5FFFF belongs to no subordinate).
//   2. permits(master, slave): the full yaml connectivity block.
//   3. inner_peripherals_claim(): the peripherals-block front crossbar.
//   4. permits(master, addr): both stages in RTL order.
//   5. name(): every enumerator has a stable, non-"?" name.
// ===========================================================================

#include "xbar_policy.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

unsigned g_checks = 0;
unsigned g_failed = 0;

template <typename T>
void expect_eq(const T& expected, const T& actual, const char* what) {
    ++g_checks;
    if (expected != actual) {
        ++g_failed;
        std::printf("  [FAIL] %s: expected %lld got %lld\n", what,
                    static_cast<long long>(expected),
                    static_cast<long long>(actual));
    }
}

void expect_slave(sep_xbar::slave expected, uint64_t addr) {
    const auto actual = sep_xbar::classify(addr);
    ++g_checks;
    if (expected != actual) {
        ++g_failed;
        std::printf("  [FAIL] classify(0x%llX): expected %s got %s\n",
                    static_cast<unsigned long long>(addr),
                    sep_xbar::name(expected), sep_xbar::name(actual));
    }
}

void expect_permit(bool expected, sep_xbar::master m, uint64_t addr) {
    const bool actual = sep_xbar::permits(m, addr);
    ++g_checks;
    if (expected != actual) {
        ++g_failed;
        std::printf("  [FAIL] permits(%s, 0x%llX): expected %d got %d\n",
                    sep_xbar::name(m), static_cast<unsigned long long>(addr),
                    expected, actual);
    }
}

void expect_permit(bool expected, sep_xbar::master m, sep_xbar::slave s) {
    const bool actual = sep_xbar::permits(m, s);
    ++g_checks;
    if (expected != actual) {
        ++g_failed;
        std::printf("  [FAIL] permits(%s, %s): expected %d got %d\n",
                    sep_xbar::name(m), sep_xbar::name(s), expected, actual);
    }
}

// Check the three edges of one window: first byte, last byte, one past.
void expect_window(sep_xbar::slave s, uint64_t base, uint64_t size,
                   sep_xbar::slave after) {
    expect_slave(s, base);
    expect_slave(s, base + size - 1);
    expect_slave(after, base + size);
}

}  // namespace

int main() {
    using sep_xbar::master;
    using sep_xbar::slave;

    std::printf("==== SEP xbar_policy TB ====\n");

    // ------------------------------------------------------------------
    // 1. classify(): yaml `outputs` address_ranges.
    // ------------------------------------------------------------------
    // iccm {0xC0000000, 0x40000} + dccm {0xC0040000, 0x20000}; 0xC0060000 is
    // outside every window.
    expect_window(slave::cpu_tcm, 0xC0000000ULL, 0x60000ULL, slave::unclassified);
    expect_slave(slave::cpu_tcm, 0xC003FFFFULL);   // last iccm byte
    expect_slave(slave::cpu_tcm, 0xC0040000ULL);   // first dccm byte

    // sram {0x10000000, 0x40000}; 0x10040000 (VP boot-ROM alias) is unclaimed.
    expect_window(slave::sram, 0x10000000ULL, 0x40000ULL, slave::unclassified);

    // dma_csr {0x10800000, 0x1000} is followed directly by sep_wdt.
    expect_window(slave::dma_csr, 0x10800000ULL, 0x1000ULL, slave::sep_wdt);
    // sep_wdt {0x10801000, 0x1000} is followed by scratch_region
    // (sep_system_peripherals).
    expect_window(slave::sep_wdt, 0x10801000ULL, 0x1000ULL,
                  slave::sep_system_peripherals);
    // scratch_region {0x10802000, 0x100}.
    expect_window(slave::sep_system_peripherals, 0x10802000ULL, 0x100ULL,
                  slave::unclassified);
    // sep_reset_ctrl {0x10803000, 0x8}.
    expect_window(slave::sep_reset_ctrl, 0x10803000ULL, 0x8ULL,
                  slave::unclassified);
    // sep_crypto {0x10900000, 0x50000} is followed directly by entropy_fifo.
    expect_window(slave::sep_crypto, 0x10900000ULL, 0x50000ULL,
                  slave::entropy_fifo);
    // entropy_fifo {0x10950000, 0x10000}.
    expect_window(slave::entropy_fifo, 0x10950000ULL, 0x10000ULL,
                  slave::unclassified);

    // csr_region after tt-oca-hw #2587: 0x10A00000..0x10A4FFFF.  The old rule
    // ran to 0x10A60000; 0x10A50000..0x10A5FFFF is now unowned (RTL DECERR).
    expect_window(slave::sep_system_peripherals, 0x10A00000ULL, 0x50000ULL,
                  slave::unclassified);
    expect_slave(slave::unclassified, 0x10A50000ULL);
    expect_slave(slave::unclassified, 0x10A58000ULL);
    expect_slave(slave::unclassified, 0x10A5FFFFULL);
    expect_slave(slave::unclassified, 0x10A60000ULL);

    // sep_io {0x10B00000, 0xFFFFF}: the yaml really says 0xFFFFF, so the last
    // byte of the megabyte is unowned.
    expect_window(slave::sep_io, 0x10B00000ULL, 0xFFFFFULL, slave::unclassified);
    expect_slave(slave::unclassified, 0x10BFFFFFULL);

    // remap_region (AP + STEE) {0x11000000, 0x1000000}.
    expect_window(slave::sep_system_peripherals, 0x11000000ULL, 0x1000000ULL,
                  slave::unclassified);
    // external_chiplet {0x00000000, 0x10000000} is followed directly by sram.
    expect_window(slave::sep_system_peripherals, 0x00000000ULL, 0x10000000ULL,
                  slave::sram);
    // sep_external {0x20000000, 0x20000000} is followed by external_smu.
    expect_window(slave::sep_external, 0x20000000ULL, 0x20000000ULL,
                  slave::sep_system_peripherals);
    // external_smu {0x40000000, 0x80000000} is followed by cpu_tcm.
    expect_window(slave::sep_system_peripherals, 0x40000000ULL, 0x80000000ULL,
                  slave::cpu_tcm);

    // Nothing above the TCMs or between the peripheral clusters is claimed.
    expect_slave(slave::unclassified, 0xC0060000ULL);
    expect_slave(slave::unclassified, 0xFFFFFFFFULL);
    expect_slave(slave::unclassified, 0x10804000ULL);
    expect_slave(slave::unclassified, 0x10C00000ULL);
    expect_slave(slave::unclassified, 0x12000000ULL);
    std::printf("  [%s] classify()\n", g_failed ? "FAIL" : "PASS");

    // ------------------------------------------------------------------
    // 2. permits(master, slave): yaml `connectivity`.
    // ------------------------------------------------------------------
    const unsigned before_matrix = g_failed;
    const slave all_slaves[] = {
        slave::cpu_tcm, slave::sram, slave::dma_csr, slave::sep_wdt,
        slave::sep_reset_ctrl, slave::sep_crypto, slave::sep_system_peripherals,
        slave::sep_io, slave::entropy_fifo, slave::sep_external,
        slave::unclassified,
    };
    // cpu (ifu_sram + lsu + dbg + VP TCM path) and local_alias reach everything.
    for (slave s : all_slaves) {
        expect_permit(true, master::cpu, s);
        expect_permit(true, master::local_alias, s);
    }
    // dma: [cpu_tcm, sram, sep_wdt, sep_reset_ctrl, sep_crypto,
    //       sep_system_peripherals, sep_io, sep_external]
    for (slave s : all_slaves) {
        const bool allowed = (s != slave::dma_csr && s != slave::entropy_fifo);
        expect_permit(allowed, master::dma, s);
    }
    // ext: [sram, dma_csr, sep_wdt, sep_crypto, sep_io, entropy_fifo,
    //       sep_external]
    for (slave s : all_slaves) {
        const bool allowed = (s != slave::cpu_tcm && s != slave::sep_reset_ctrl &&
                              s != slave::sep_system_peripherals);
        expect_permit(allowed, master::ext, s);
    }
    std::printf("  [%s] permits(master, slave)\n",
                g_failed != before_matrix ? "FAIL" : "PASS");

    // ------------------------------------------------------------------
    // 3. inner_peripherals_claim(): mailbox + system_csr + scratch_region.
    // ------------------------------------------------------------------
    const unsigned before_inner = g_failed;
    expect_eq(true,  sep_xbar::inner_peripherals_claim(0x10A00000ULL), "inner mailbox first");
    expect_eq(true,  sep_xbar::inner_peripherals_claim(0x10A0FFFFULL), "inner mailbox last");
    expect_eq(true,  sep_xbar::inner_peripherals_claim(0x10A10000ULL), "inner system_csr first");
    expect_eq(true,  sep_xbar::inner_peripherals_claim(0x10A4FFFFULL), "inner system_csr last");
    expect_eq(false, sep_xbar::inner_peripherals_claim(0x10A50000ULL), "inner one past system_csr");
    expect_eq(false, sep_xbar::inner_peripherals_claim(0x10A5FFFFULL), "inner trimmed csr_region tail");
    expect_eq(true,  sep_xbar::inner_peripherals_claim(0x10802000ULL), "inner scratch first");
    expect_eq(true,  sep_xbar::inner_peripherals_claim(0x108020FFULL), "inner scratch last");
    expect_eq(false, sep_xbar::inner_peripherals_claim(0x10802100ULL), "inner one past scratch");
    expect_eq(false, sep_xbar::inner_peripherals_claim(0x109FFFFFULL), "inner below mailbox");
    expect_eq(false, sep_xbar::inner_peripherals_claim(0x11000000ULL), "inner remap_region");
    std::printf("  [%s] inner_peripherals_claim()\n",
                g_failed != before_inner ? "FAIL" : "PASS");

    // ------------------------------------------------------------------
    // 4. permits(master, addr): inner stage first, then the local matrix.
    // ------------------------------------------------------------------
    const unsigned before_addr = g_failed;
    // Inner stage lets any master at the mailbox / system_csr / scratch.
    expect_permit(true, master::ext, 0x10A00000ULL);   // mailbox
    expect_permit(true, master::ext, 0x10A4FFFFULL);   // last system_csr byte
    expect_permit(true, master::ext, 0x10802000ULL);   // scratch_region
    expect_permit(true, master::dma, 0x10A10000ULL);
    // Local matrix denials for ext.
    expect_permit(false, master::ext, 0xC0000000ULL);  // cpu_tcm
    expect_permit(false, master::ext, 0xC005FFFFULL);  // last dccm byte
    expect_permit(false, master::ext, 0x10803000ULL);  // sep_reset_ctrl
    expect_permit(false, master::ext, 0x11000000ULL);  // remap_region
    expect_permit(false, master::ext, 0x00000000ULL);  // external_chiplet
    expect_permit(false, master::ext, 0x40000000ULL);  // external_smu
    // Local matrix grants for ext.
    expect_permit(true, master::ext, 0x10000000ULL);   // sram
    expect_permit(true, master::ext, 0x10800000ULL);   // dma_csr
    expect_permit(true, master::ext, 0x10801000ULL);   // sep_wdt
    expect_permit(true, master::ext, 0x10900000ULL);   // sep_crypto
    expect_permit(true, master::ext, 0x10950000ULL);   // entropy_fifo
    expect_permit(true, master::ext, 0x10B00000ULL);   // sep_io
    expect_permit(true, master::ext, 0x20000000ULL);   // sep_external
    // Local matrix for dma.
    expect_permit(false, master::dma, 0x10800000ULL);  // dma_csr
    expect_permit(false, master::dma, 0x10950000ULL);  // entropy_fifo
    expect_permit(true,  master::dma, 0xC0000000ULL);  // cpu_tcm
    expect_permit(true,  master::dma, 0x10803000ULL);  // sep_reset_ctrl
    // cpu / local_alias reach everything.
    expect_permit(true, master::cpu,         0x10800000ULL);
    expect_permit(true, master::cpu,         0x10A50000ULL);
    expect_permit(true, master::local_alias, 0xC0000000ULL);
    expect_permit(true, master::local_alias, 0x10803000ULL);
    // The #2587 tail is unowned: the policy no longer classifies it as
    // sep_system_peripherals, so it is not an ext matrix denial any more.
    // It has no subordinate, so the bus decode (not this policy) rejects it.
    expect_permit(true, master::ext, 0x10A50000ULL);
    expect_permit(true, master::ext, 0x10A5FFFFULL);
    // VP-only conveniences outside every RTL window are permitted for all.
    expect_permit(true, master::ext, 0x10040000ULL);   // boot ROM alias
    expect_permit(true, master::dma, 0xC0060000ULL);
    std::printf("  [%s] permits(master, addr)\n",
                g_failed != before_addr ? "FAIL" : "PASS");

    // ------------------------------------------------------------------
    // 5. name(): stable strings for every enumerator.
    // ------------------------------------------------------------------
    const unsigned before_name = g_failed;
    const char* const slave_names[] = {
        "cpu_tcm", "sram", "dma_csr", "sep_wdt", "sep_reset_ctrl", "sep_crypto",
        "sep_system_peripherals", "sep_io", "entropy_fifo", "sep_external",
        "unclassified",
    };
    for (size_t i = 0; i < sizeof(all_slaves) / sizeof(all_slaves[0]); ++i) {
        ++g_checks;
        if (std::string(sep_xbar::name(all_slaves[i])) != slave_names[i]) {
            ++g_failed;
            std::printf("  [FAIL] name(slave %zu) = %s\n", i,
                        sep_xbar::name(all_slaves[i]));
        }
    }
    const master all_masters[] = {master::cpu, master::dma, master::local_alias,
                                  master::ext};
    const char* const master_names[] = {"cpu", "dma", "local_alias", "ext"};
    for (size_t i = 0; i < 4; ++i) {
        ++g_checks;
        if (std::string(sep_xbar::name(all_masters[i])) != master_names[i]) {
            ++g_failed;
            std::printf("  [FAIL] name(master %zu) = %s\n", i,
                        sep_xbar::name(all_masters[i]));
        }
    }
    // Out-of-range enumerators fall through to "?" (defensive path).
    ++g_checks;
    if (std::string(sep_xbar::name(static_cast<slave>(99))) != "?") {
        ++g_failed;
        std::printf("  [FAIL] name(slave 99) != \"?\"\n");
    }
    ++g_checks;
    if (std::string(sep_xbar::name(static_cast<master>(99))) != "?") {
        ++g_failed;
        std::printf("  [FAIL] name(master 99) != \"?\"\n");
    }
    ++g_checks;
    if (sep_xbar::permits(static_cast<master>(99), slave::cpu_tcm) != true) {
        ++g_failed;
        std::printf("  [FAIL] permits(master 99, cpu_tcm) != true\n");
    }
    std::printf("  [%s] name()\n", g_failed != before_name ? "FAIL" : "PASS");

    std::printf("==== %u checks, %u failed ====\n", g_checks, g_failed);
    std::printf(g_failed ? "SOME TESTS FAILED\n" : "ALL TESTS PASSED\n");
    return g_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
