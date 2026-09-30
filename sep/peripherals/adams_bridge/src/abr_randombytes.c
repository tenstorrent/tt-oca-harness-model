// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file abr_randombytes.c
 * @brief Stub for PQClean's randomized public APIs.
 *
 * Adams Bridge always uses the derandomized internals (seed / mu / rnd / m
 * come from MMIO). The randomized wrappers remain in the object files and
 * need a linker symbol; calling them is a programming error.
 */

#include "randombytes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* LCOV_EXCL_START — PQClean's randomized entry point is never called: the ABR
 * backend uses only the derandomized (seeded) APIs, so this stub exists purely
 * to satisfy the link. */
int randombytes(uint8_t *output, size_t n)
{
    /* Never reached by the FIPS backend. Fill so a stray call is obvious. */
    if (output != NULL && n > 0u) {
        memset(output, 0xA5, n);
    }
    fputs("abr: PQClean randombytes() stub invoked; use derand APIs\n", stderr);
    return -1;
}
/* LCOV_EXCL_STOP */
