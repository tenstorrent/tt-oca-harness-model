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

int randombytes(uint8_t *output, size_t n) // GCOV_EXCL_LINE
{
    /* Never reached by the FIPS backend. Fill so a stray call is obvious. */
    if (output != NULL && n > 0u) { // GCOV_EXCL_LINE
        memset(output, 0xA5, n);    // GCOV_EXCL_LINE
    }                               // GCOV_EXCL_LINE
    fputs("abr: PQClean randombytes() stub invoked; use derand APIs\n", stderr); // GCOV_EXCL_LINE
    return -1; // GCOV_EXCL_LINE
}
