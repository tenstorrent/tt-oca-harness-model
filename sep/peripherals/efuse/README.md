# efuse

SystemC TLM2.0 model of the SEP eFuse block: the OTP array, its shadow register map,
the program/read interface, the lock policy, and SHA-256 token matching. The lifecycle
controller reads fuse state from this model at reset to determine the current device
lifecycle.

The model is behavioural, not a stub. An 8192-bit array is the source of truth, and the
shadow registers are the same bits viewed as words — a field reads back identically
whether software goes through the shadow map or through the OTP read CSR. Fuse contents
come from a `.preload` image, from per-field CCI parameters, or both; parameters are ORed
over the image, because fuses only ever go 0→1.

What is deliberately not modelled: cycle-level timing. The program and read handshakes
and the token hash all complete inside the write that starts them, so software sees
`done` on its next read and never observes the busy window. Nothing else about the
sequence is skipped — status, error and data all behave as they do in silicon.

## Files

```
model/inc/efuse_base.h        Register map and TLM socket base
model/inc/efuse.h             efuse_model class declaration
model/src/efuse_base.cpp      Base construction and register binding
model/src/efuse.cpp           CCI parameter binding and b_transport handler

test/inc/testbench.h          Testbench module header
test/inc/efuse_basetest.h     Base test class
test/inc/efuse_test.h         Test case declarations
test/src/testbench.cpp        sc_main entry
test/src/efuse_basetest.cpp   Common test infrastructure
test/src/efuse_test.cpp       Test orchestration
```

## Address

Two disjoint windows, as in the register map:

| Window | Range | Contents | Socket |
|---|---|---|---|
| `sep_efuse` | `0x10930000 – 0x1093056F` | shadow map, `EFUSE_INTERFACE_CTRL`, `EFUSE_MMR` | `target_socket` |
| `SEP_EXTERNAL` | `0x20000000 – 0x20000043` | `EFUSE_SHIM_CTRL` | `shim_target_socket` |

The split follows the hardware: `efuse_interface_controller.sv` decodes
`[EFUSE_MAP_REG_MAP_BASE_ADDR : EFUSE_MMR_REG_MAP_END_ADDR]` onto its internal APB
path and routes everything else on its slave to the shim's own AXI-Lite port, which
`sep.sv` feeds by demuxing `0x20000000` out of `SEP_EXTERNAL`.

## Behaviour

**Fuse array.** 8192 bits, 256 words, bit-addressable through `EFUSE_WRITE_CTRL` /
`EFUSE_READ_CTRL`. Programming is one-way: the Samsung macro ORs new data over old
(`d_latch = din | fuse`), so a burned bit cannot be cleared, and programming data=0 is
refused outright rather than treated as a no-op. Out-of-range addresses latch their own
sticky error, separate from the generic request error.

**Locks.** `LOCKS_LO` / `LOCKS_HI` hold a write-lock/read-lock pair per field, write-1-to-set
and sticky until reset. Both paths are gated, and they refuse differently, as the hardware
does: a shadow access returns `0xBADCAB1E` with a normal bus response, while an OTP command
is dropped before it reaches the macro and reports done-with-error plus a sticky
`req_error`. A single pair covers all eight words of the multi-word keys and tokens.
`LOCKS` itself is never lockable, so software can always see what it has locked.

**Token matching.** Software stages a 256-bit token in eight input words and pulses a go
bit in `TOKEN_EOP`. The model hashes it with SHA-256 — most significant token word first,
big-endian, no salt or length prefix — and compares the digest against a reference,
publishing `0x15` (match) or `0x2A` (mismatch). Both sides of the comparison are digests;
the plaintext token is never compared. A match is what authorises burning the
corresponding `LC_STATE` RMA advance bit, so those two bits are refused without it.

The RMA references are fuse-backed. Note the naming: RTL calls those fields
`RMA_SIP_TOKEN_DIGEST` and `RMA_CHIPLET_TOKEN_DIGEST`, and they hold digests despite the
shorter `rma_sip_token` / `rma_chiplet_token` parameter names here. Secure disable has no
fuse at all — in silicon it is the `SEP_SEC_DISABLE_TOKEN` RTL parameter, built from rev
cells so it can be changed by a metal-only ECO, with the production value embedded at
synthesis. The `sec_disable_token_digest` parameter stands in for it and defaults to zero
exactly as the RTL parameter does, which means no token matches until a config supplies a
digest.

`0x3F` (tamper) is never produced: it requires a fault in the redundant comparison lanes
that the model has no way to inject.

## Class

```cpp
class efuse_model : public efuse_base
```

## Interface

| Port / Socket | Direction | Description |
|---|---|---|
| `target_socket` | target | TLM-2.0 32-bit register bus, `sep_efuse` window |
| `shim_target_socket` | target | TLM-2.0 32-bit register bus, `EFUSE_SHIM_CTRL` window |
| `rst_ni` | `sc_in<bool>` | Active-low reset |

## CCI Configuration (efuse_vp.ini)

The array is set at elaboration, either from a whole-array image or from per-field
parameters. These are alternatives, not a base and an overlay: when
`fuse_preload_file` is set the image defines the array and the per-field parameters
are ignored.

```ini
# Whole-array image in the RTL's +sep_preload_efuse format: 8192 lines, one ASCII
# '0' or '1' each, LSB first. A relative path is relative to the .ini naming it.
# Empty (the default) leaves the array erased, matching +SEP_EFUSE_NO_PRELOAD.
och_sep_ss1.sep_efuse.fuse_preload_file : ../../path/to/default_efuse.preload

# Per-field alternative, for a standalone testbench with no image to point at.
och_sep_ss1.sep_efuse.lc_state    : 1             # raw lifecycle state, encoded on load
och_sep_ss1.sep_efuse.chiplet_uid : [1, 2, 3, 4, 5, 6, 7, 8]
och_sep_ss1.sep_efuse.locks_lo    : 43008         # 0xA800

# Reference digest for secure disable; word 0 is digest bits [31:0]. Not fuse-backed,
# so it applies either way.
och_sep_ss1.sep_efuse.sec_disable_token_digest : [224340261, 2418694429, 1860318131, 144118917, 2392821280, 1821360523, 4167220599, 1718123181]
```

Why the two cannot be combined: combining could only mean ORing, since fuses go 0->1
and assigning would let a parameter left at its default of 0 unburn a field the image
had set. But ORing two encodings of one field corrupts it. `LC_STATE` shows this
plainly — the fuse holds `{~raw[3:0], raw[3:0]}`, so an image holding 0xF0 (TEST_DEV)
ORed with the encoding of raw 1 gives 0xF1, which is not a legal code at all and leaves
the part with no valid lifecycle state. To run with different fuses, change the image.

Two things follow from that encoding. `lc_state` is configured as the raw 4-bit value
and encoded on the way into the array, so configs stay readable and match the encodings
`lifecycle_ctrl` compares against. And an erased array reads 0x00, which is *not* a
legal code, so a blank part has no valid lifecycle state — true of the RTL too, and
`get_lc_state()` reports it as INVALID rather than as TEST_DEV.

A lock parameter is not a formality — `locks_lo` is enforced, so a config that sets it
will make the corresponding shadow reads return the deny word.

## Building and Testing

```bash
# Using run_tests.sh (recommended)
./run_tests.sh              # Release build + run
./run_tests.sh --debug      # Debug build
./run_tests.sh --asan       # AddressSanitizer
./run_tests.sh --coverage   # lcov coverage report
./run_tests.sh --ctest      # via CTest (verbose)
./run_tests.sh --clean      # clean build dir first
./run_tests.sh --cppcheck   # for static analysis

# Manual CMake
mkdir -p build/debug && cd build/debug
cmake ../.. -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
./bin/efuse_test
```

## Documentation

[High-Level Design](docs/design-docs/efuse-high-level-design.md)
