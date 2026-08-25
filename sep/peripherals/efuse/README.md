# efuse

SystemC TLM2.0 model of the SEP eFuse block: the OTP array, its shadow register map,
the program/read interface, the lock policy, and SHA-256 token matching. The lifecycle
controller reads fuse state from this model at reset to determine the current device
lifecycle.

The model is behavioural, not a stub. An 8192-bit array is the source of truth, and the
shadow registers are the same bits viewed as words — a field reads back identically
whether software goes through the shadow map or through the OTP read CSR. Fuse contents
come from either a preload image or per-field CCI parameters. When
`fuse_preload_file` is set and loads successfully, the image defines the array and
per-field parameters are not applied. Either image format is accepted, detected from the
file rather than declared: the OCA testbench's one-ASCII-bit-per-line `.preload`, or the
harness DV's `$readmemh` hex words with optional `@addr` origins and `//` comments.

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

doc/index.adoc                VP set entry (includes the two pages)
doc/implementation.adoc       SystemC/TLM model
doc/test_plan.adoc            cases + run commands
```

## Address

Two disjoint windows, as in the register map:

| Window | Range | Contents | Socket |
|---|---|---|---|
| `sep_efuse` | `0x10930000 – 0x1093056F` | shadow map, `EFUSE_INTERFACE_CTRL`, `EFUSE_MMR` | `target_socket` |
| `SEP_EXTERNAL` | `0x20000000 – 0x20000003` | `EFUSE_SHIM_CTRL` (one register) | `shim_target_socket` |

The split follows the hardware: `efuse_interface_controller.sv` decodes
`[EFUSE_MAP_REG_MAP_BASE_ADDR : EFUSE_MMR_REG_MAP_END_ADDR]` onto its internal APB
path and routes everything else on its slave to the shim's own AXI-Lite port, which
`sep.sv` feeds by demuxing `0x20000000` out of `SEP_EXTERNAL`.

## Behaviour

**Fuse array.** 8192 bits, 256 words, bit-addressable through `EFUSE_PROGRAM_CTRL` /
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

**Transient RMA.** With the `TRANSIENT_RMA_EN` fuse burned, a token match advances
`LC_STATE` on its own, with no software write: SiP sets bit [1], chiplet sets bit [2].
The guards are the ones the software path enforces — the mechanism removes the need for
the write, not the need for the token. Two details worth knowing, both faithful to
`efuse_shadow_regs.sv`: a matched chiplet token whose guard fails *blocks* rather than
falling through to the SiP transition, and match results are sticky, so a chiplet match
left over from an earlier comparison keeps blocking until it is re-evaluated.

**Locked-field interrupt.** A refused shadow access — a read of a read-locked field or a
write to a write-locked one — pulses `locked_field_access_irq_o`. The access still
completes on the bus, so this pin is the only notification; there is no status bit and
nothing to clear. It covers the shadow path only. A refused *fuse command* is a different
mechanism (`efuse_guard`) and reports through `req_error` instead.

**Secure test mode.** With `secure_tm` set, every fuse command is dropped and the secrets
handed to the key manager read as zero, while the shadow bus is untouched: the gate is on
the hardware ports and the OTP interface, not on the read path. One divergence, and it is
forced: in silicon the dropped command's response is tied off, so the interface stalls
until its request timeout. `b_transport` cannot stall without wedging the kernel, so the
command completes immediately with `done` and no error — RTL is explicit that a block is
not an error capture. Nothing is burned and nothing is read either way.

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
| `locked_field_access_irq_o` | `sc_out<bool>` | Pulses when the locks refuse a shadow access |

## CCI Configuration (efuse_vp.ini)

The array is set at elaboration, either from a whole-array image or from per-field
parameters. These are alternatives, not a base and an overlay: when
`fuse_preload_file` is set the image defines the array and the per-field parameters
are ignored.

```ini
# Whole-array image, in either the +sep_preload_efuse format (8192 lines, one ASCII
# '0' or '1' each, LSB first) or a $readmemh image (256 hex words, word 0 first).
# The format is detected from the file. A relative path is relative to the .ini
# naming it. Empty (the default) leaves the array erased, as +SEP_EFUSE_NO_PRELOAD does.
och_sep_ss1.sep_efuse.fuse_preload_file : ../../path/to/default_efuse.preload

# The test_en strap. Blocks every fuse command and zeroes the key manager's secrets;
# lc_ctrl reads it from here on a platform, so this one knob covers both.
och_sep_ss1.sep_efuse.secure_tm : false

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

Architecture, CSRs, and programming sequences are in the hardware
TRM. This tree documents the SystemC/TLM model:

- [index](doc/index.adoc) — VP set entry
- [implementation](doc/implementation.adoc) — sockets, threads, CCI, gaps
- [test plan](doc/test_plan.adoc) — standalone cases and platform runs
