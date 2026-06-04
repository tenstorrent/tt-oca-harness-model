# OpenTitan HMAC Test for SEP Platform

This directory contains the ported OpenTitan HMAC smoketest, adapted to run on the SEP RISC-V VP++ platform.

## Quick Start

### Build
```bash
make clean
make
```

### Run
```bash
make sim
```

## What This Test Does

This test validates HMAC peripheral functionality by:
1. Running SHA-256 hash operations (2 passes)
2. Running HMAC operations with a key (2 passes)
3. Verifying digest outputs match expected values

## Test Vectors

The test uses the same test vectors as the original OpenTitan HMAC smoketest:

**Input Message:**
```
"Every one suspects himself of at least one of the cardinal virtues, 
and this is mine: I am one of the few honest people that I have ever known"
```

**Expected SHA-256 Digest:**
```
0xd6c6c94e 0xf7cff519 0x45c76d42 0x9d37a8b8
0xe2762fe9 0x71ff68cb 0x68e236af 0x3dc296dc
```

**Expected HMAC Digest (with key):**
```
0xebce4019 0x284d39f1 0x5eae12b0 0x0c48fb23
0xfadb9531 0xafbbf3c2 0x90d3833f 0x397b98e4
```

## Files

- `main.c` - Ported test code from OpenTitan
- `start.S` - Startup assembly code
- `link.ld` - Linker script for SEP platform memory map
- `Makefile` - Build configuration

## Compatibility Layer

This test uses the OpenTitan compatibility layer located in `../opentitan-compat/`.
See the [compatibility layer README](../opentitan-compat/README.md) for details.

## Original Source

Ported from: `opentitan/sw/device/tests/hmac_smoketest.c`

## Build Output

- **ELF File:** `opentitan-hmac-test` (24KB)
- **Architecture:** RV32I
- **Memory Usage:** ~2KB SRAM
