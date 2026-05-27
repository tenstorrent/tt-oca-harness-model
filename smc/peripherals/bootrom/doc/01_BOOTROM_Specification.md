# SEP Boot ROM — Functional Specification

**Document**: `01_BOOTROM_Specification.md`
**Module**: `smc::bootrom` (SystemC/TLM-2.0 Loosely-Timed model, CCI-parameterised)
**Spec base**: `tt-oca-hw/meta/registers/rdl/sep_boot_rom.rdl`
**Status**: Frozen for SEP bring-up; tracks the cocotb conformance suite under
            `tt-oca-hw/dv/oss/shims/sep/memories/tests/conformance/`
**Companion docs**:
  - `02_BOOTROM_LowLevel_Design.md` — internal SystemC implementation
  - `03_BOOTROM_Test_Plan.md` — verification strategy & test list

---

## Contents

1. [Purpose & scope](#purpose-scope)
2. [Conformance & references](#conformance-references)
3. [Feature summary](#feature-summary)
4. [Configuration parameters](#configuration-parameters)
5. [Block diagram & port list](#block-diagram-port-list)
6. [Theory of operation](#theory-of-operation)
7. [Memory map](#memory-map)
8. [Functional behaviour](#functional-behaviour)
9. [Reset behaviour](#reset-behaviour)
10. [Bus interface (TLM-2.0 / AXI4-style)](#bus-interface-tlm-2.0-axi4-style)
11. [Error handling](#error-handling)
12. [Preload formats](#preload-formats)
13. [Compliance matrix](#compliance-matrix)
14. [Revision history](#revision-history)
15. [Glossary](#glossary)

---

## 1. Purpose & scope

The **SEP Boot ROM** is a non-volatile, preloaded, read-only memory that
holds the first instructions executed by the SEP (Secure Entry
Processor) after reset. It is functionally trivial — software cannot
modify it — but it is on the critical bring-up path: a bug in the
preload pipeline manifests as "the SEP refuses to boot", which is one
of the hardest classes of platform failure to debug in simulation.

This specification defines the **externally-observable behaviour** of
the SEP Boot ROM SystemC model: configuration, memory map, access
semantics, error responses, reset, and bus contract. It is the contract
that:

- The SEP core fetches its very first instruction against,
- The cocotb conformance suite measures, and
- Any future RTL or platform integration treats as authoritative.

Internal implementation choices (data layout, preload parser, the
CCI-parameter scaffolding, single-driver discipline) are documented in
[`02_BOOTROM_LowLevel_Design.md`](02_BOOTROM_LowLevel_Design.md).

---

## 2. Conformance & references

| Source | Authority |
|--------|-----------|
| `tt-oca-hw/meta/registers/rdl/sep_boot_rom.rdl` | **Ground-truth memory map** (`mem`-type, 64-bit wide, `sw=r, hw=r`) |
| `tt-oca-hw/dv/oss/shims/sep/memories/tests/conformance/test_sep_boot_rom_rw.py` | Read/write conformance claims **C2–C4** |
| `tt-oca-hw/dv/oss/shims/sep/memories/tests/conformance/test_sep_boot_rom_preload.py` | Preload-format conformance |
| `tt-oca-hw/hw/smc/data/scripts/bootrom.rv64.img` | Sample binary preload image (raw little-endian 8-byte words) |
| `tt-oca-hw/dv/smc/tb/meta/scripts/bootrom_sanity.rv64.hex` | Sample hex preload (one 64-bit big-endian ASCII word per line) |
| `01_PLIC_Specification.md`, `01_CLINT_Specification.md` | Sister SMC IPs — same conventions |

The SystemC model is **byte-identical** to the RDL memory layout: a
contiguous `NUM_ENTRIES`-by-8-byte memory at offset 0, addressable at
any naturally-aligned `{1, 2, 4, 8}`-byte width.

---

## 3. Feature summary

| Feature                            | Value / behaviour                                            |
|------------------------------------|--------------------------------------------------------------|
| Standard                           | OCA SEP boot ROM; structurally identical to a `mem` block in RDL |
| Size                               | 64 KiB (default; configurable via CCI `size_bytes`)          |
| Native word width                  | 64 bits (matches `regwidth = memwidth = 64` in the RDL)      |
| Supported access widths            | 1, 2, 4, 8 bytes (naturally aligned)                         |
| Software write semantics           | **Silently ignored** — `TLM_OK_RESPONSE`, contents unchanged |
| Read-after-write                   | Returns the unchanged preloaded value (never the attempted write) |
| Preload formats                    | Hex (text, big-endian) or binary (raw little-endian)         |
| Zero-init behaviour                | When `init_file` is empty: all bytes read as `0x00`          |
| Bus interface                      | TLM-2.0 LT target socket (AXI4-style)                        |
| DMI                                | Not granted (kept consistent with other SMC IPs; future work) |
| Reset polarity                     | Active-low, synchronous; structural no-op (ROM has no state to clear) |
| Configuration interface            | SystemC CCI 1.0 (`cci::cci_param<T>`)                        |
| Annotated TLM delay                | 1.0 ns default; mutable via the `access_delay_ns` CCI param   |

---

## 4. Configuration parameters

The model exposes **four real `cci::cci_param<T>` declarations**. The
size and preload parameters are immutable (changing them after
elaboration would invalidate the in-memory image); the access delay is
mutable so simulations can sweep bus speeds without rebuilding.

| Parameter           | Type          | Default      | Mutability | Notes                                                                          |
|---------------------|---------------|--------------|------------|--------------------------------------------------------------------------------|
| `size_bytes`        | `uint64_t`    | `0x10000`    | immutable  | Must be a non-zero multiple of 8 (the native 64-bit word width).               |
| `init_file`         | `std::string` | `""`         | immutable  | Path to preload image; empty = zero-fill (`test_preload_zero_init` claim).     |
| `init_file_format`  | `std::string` | `"auto"`     | immutable  | `"hex"`, `"bin"`, or `"auto"` (pick `bin` for `.img`/`.bin`, else `hex`).      |
| `access_delay_ns`   | `double`      | `1.0`        | mutable    | TLM `b_transport` annotated delay. Claim C2 (1-cycle `rvalid`) ⇒ ≈ 1 ns.       |

Override before construction via the CCI broker:

```cpp
broker.set_preset_cci_value("sep.bootrom.size_bytes",
                            cci::cci_value(uint64_t(0x10000)));
broker.set_preset_cci_value("sep.bootrom.init_file",
                            cci::cci_value(std::string("bootrom.hex")));
broker.set_preset_cci_value("sep.bootrom.init_file_format",
                            cci::cci_value(std::string("hex")));
broker.set_preset_cci_value("sep.bootrom.access_delay_ns",
                            cci::cci_value(2.0));
```

The constructor still accepts a `bootrom_cfg` struct; its fields supply
**defaults** for the corresponding CCI params, so old code that does
not use the CCI broker continues to work unchanged.

Fixed structural constants:

| Constant                | Value | Description                                       |
|-------------------------|-------|---------------------------------------------------|
| `WORD_BYTES`            | 8     | Native ROM word width (from RDL `memwidth = 64`)   |
| `MAX_ACCESS_BYTES`      | 8     | Largest single `b_transport` access width accepted |

---

## 5. Block diagram & port list

```
                ┌────────────────────────────────────────┐
                │             smc::bootrom               │
                │ ┌──────────────────────────────────┐   │
   reg_socket   │ │  data_[size_bytes]               │   │
   ─────────►   │ │  (preloaded byte array)          │   │
   AXI4-style   │ └──────────────────────────────────┘   │
   target       │     ▲  reads (1/2/4/8 B)               │
                │     │  writes silently ignored         │
                │                                         │
   rst_n_i ───► │  reset_proc (no-op; structural only)   │
                └────────────────────────────────────────┘
```

| Port         | Direction | Type                                  | Description |
|--------------|-----------|---------------------------------------|-------------|
| `reg_socket` | target    | `tlm_utils::simple_target_socket<bootrom>` | TLM-2.0 target socket; AXI4-style read access |
| `rst_n_i`    | input     | `sc_in<bool>`                         | Active-low synchronous reset (no-op; kept for IP-suite symmetry) |

The Boot ROM has **no output ports**. It is a passive memory.

---

## 6. Theory of operation

The Boot ROM is the simplest possible target IP. It owns a single
contiguous byte array (`size_bytes` long, default 64 KiB) loaded from a
preload file at elaboration time, and serves `b_transport` reads
verbatim from that array. Writes — both production (`b_transport`) and
debug (`transport_dbg`) — are silently discarded; the contents cannot
be mutated through the bus, exactly as the underlying RDL declares
(`sw=r, hw=r`).

Three properties make this trivial-looking IP non-trivial in practice:

1. **Preload format coupling.** The SEP bring-up flow uses two
   different preload formats — raw binary `.img` and ASCII hex `.hex` —
   depending on which point in the pipeline the image was emitted.
   Both must be supported, and both must produce **byte-identical**
   in-memory images so the SEP core sees the same instruction bytes
   regardless of which format the integrator chose.
2. **Sparse-non-zero overflow tolerance.** Hex preload files generated
   by the SMC tooling are often sized for a larger ROM than the one
   they are loaded into; the tail bytes are zero-padded. The model
   accepts a longer-than-`size_bytes` hex file **iff** the overflow
   region is entirely zero — the equivalent of `xxd -r -p` truncating
   to the destination size. A non-zero overflow word is flagged as a
   configuration error (`SC_REPORT_FATAL`) because it indicates the
   image was sized for a different platform.
3. **Write-ignore vs. write-error.** The cocotb conformance test
   `test_rom_write_ignored` asserts that a write to the ROM must
   **return `rvalid` (TLM_OK_RESPONSE)** *and* leave the contents
   unchanged. Returning a bus error on a write would be incorrect — it
   would deadlock the SEP boot path. The model therefore distinguishes
   write-ignore (in-range writes; OK + no-mutation) from real bus
   errors (out-of-window, misaligned, unsupported width).

---

## 7. Memory map

All offsets are byte offsets relative to the Boot ROM base address.

| Range                              | Region                | Access | Size                                     |
|------------------------------------|-----------------------|--------|------------------------------------------|
| `0x00000` .. `size_bytes − 1`      | `mem_array[0..N − 1]` | R+W*   | 8 B × `N` (default N = 0x2000)            |
| ≥ `size_bytes`                     | Out-of-window         | —      | `TLM_ADDRESS_ERROR_RESPONSE`              |

*Write access is silently ignored (W = "write request accepted; data
discarded"). See §8.2.

The default `size_bytes = 0x10000` matches the RDL declaration
`NUM_ENTRIES = 0x2000` (8192 entries × 8 bytes = 64 KiB).

---

## 8. Functional behaviour

### 8.1 Reads

A read at byte offset `O` with width `W ∈ {1, 2, 4, 8}` returns the
byte slice `data_[O..O+W − 1]`. Multi-byte reads honour native CPU
endianness — the model never byte-swaps; the preload format determines
whether the on-disk image is little- or big-endian (see §12).

| Property              | Behaviour                                  |
|-----------------------|--------------------------------------------|
| Alignment             | Naturally aligned to `W` bytes              |
| Width range           | `W ∈ {1, 2, 4, 8}`                          |
| Response status       | `TLM_OK_RESPONSE` on success                |
| Annotated delay       | `+= access_delay_ns × ns` (claim C2)        |
| Side effects          | None — reads never mutate state             |

### 8.2 Writes

A write at byte offset `O` with width `W` and in-range `O + W ≤ size_bytes`:

| Property              | Behaviour                                  |
|-----------------------|--------------------------------------------|
| Effect on `data_`     | **None** (silently discarded)              |
| Response status       | `TLM_OK_RESPONSE` (claim C4)                |
| Annotated delay       | `+= access_delay_ns × ns`                   |
| Subsequent read       | Returns the unchanged preloaded value       |

This is the only IP in the SMC suite whose write path is a no-op-by-
design rather than an error path; the firmware driver and the SEP
fabric must see `rvalid` so the bus does not stall.

### 8.3 Preload contract

At construction time, if `init_file` is non-empty, the file is opened
and decoded according to the resolved format (see §12). The decoded
bytes are stored starting at offset 0; any remaining bytes are left at
their initial value (zero). If `init_file` is empty the entire ROM is
zero-initialised — matching the cocotb `test_preload_zero_init` claim.

Preload errors (file not found, unreadable, malformed) are reported
with `SC_REPORT_FATAL` at elaboration time, so an integration mistake
fails fast rather than producing silent corruption at run time.

### 8.4 Reset behaviour

Reset is a **structural no-op**: the ROM holds no mutable state, so an
assertion of `rst_n_i` does not alter `data_`. The `reset_proc`
SC_METHOD is still registered for IP-suite symmetry — every other SMC
IP exposes a `rst_n_i`, so removing it here would break uniform binding
patterns at the platform top level.

A read immediately after reset returns the preloaded value, as the
cocotb `test_read_after_reset` claim mandates.

---

## 9. Reset behaviour

| Signal      | Polarity   | Edge          | Effect on ROM state |
|-------------|------------|---------------|---------------------|
| `rst_n_i`   | active-low | level-low     | **None** (ROM is immutable) |

A wired-low `rst_n_i` does not stall the bus interface — reads continue
to return preloaded data throughout the reset window. The cocotb
conformance harness exploits this by issuing the very first read
immediately after the reset de-asserts and expecting valid data on the
next cycle.

---

## 10. Bus interface (TLM-2.0 / AXI4-style)

### 10.1 Socket type

```cpp
tlm_utils::simple_target_socket<bootrom> reg_socket;
```

Registered callbacks:

```cpp
reg_socket.register_b_transport  (this, &bootrom::b_transport);
reg_socket.register_transport_dbg(this, &bootrom::transport_dbg);
```

DMI is **deliberately not registered**. A read-only ROM is a perfect
DMI candidate; we keep it un-granted for parity with the other SMC IPs
(see `02_BOOTROM_LowLevel_Design.md §16` for the future-work item).

### 10.2 Transaction contract

| Field             | Required value                            | Notes |
|-------------------|-------------------------------------------|-------|
| `command`         | `TLM_READ_COMMAND` / `TLM_WRITE_COMMAND`  | `TLM_IGNORE_COMMAND` → `TLM_COMMAND_ERROR_RESPONSE` |
| `address`         | `4-byte aligned to data_length, < size_bytes` | Otherwise error (see §11) |
| `data_length`     | `1`, `2`, `4`, or `8`                     | Otherwise `TLM_BURST_ERROR_RESPONSE` |
| `streaming_width` | == `data_length`                          | Otherwise `TLM_BURST_ERROR_RESPONSE` |
| `byte_enable_ptr` | `nullptr` (or `byte_enable_length == 0`)  | Non-null → `TLM_BYTE_ENABLE_ERROR_RESPONSE` |
| `dmi_allowed`     | (cleared on every transaction)            | DMI never granted |

### 10.3 Timing annotation

`b_transport` adds `sc_time(access_delay_ns_p_.get_value(), SC_NS)` to
the supplied `delay` reference. The model re-reads the CCI param on
**every** transaction, so a broker-driven mutation takes effect on the
next access.

`transport_dbg` adds nothing — debug accesses are zero-time by TLM-2.0
convention.

The Boot ROM never initiates traffic; it has no quantum keeper and
participates in whatever quantum the caller is running.

### 10.4 `transport_dbg`

Symmetric to `b_transport` for the validation path. Debug **reads**
copy from `data_` exactly like production reads; debug **writes** are
silently discarded (same contract as `b_transport`). On validation
error `transport_dbg` returns 0; on success it returns the number of
bytes transferred (TLM-2.0 convention).

---

## 11. Error handling

| Condition                                       | Detection                | Response                            |
|-------------------------------------------------|--------------------------|-------------------------------------|
| `data_length` ∉ `{1, 2, 4, 8}`                  | `b_transport` precheck   | `TLM_BURST_ERROR_RESPONSE`          |
| Address not naturally aligned to `data_length`  | `b_transport` precheck   | `TLM_BURST_ERROR_RESPONSE`          |
| `streaming_width ≠ data_length`                 | `b_transport` precheck   | `TLM_BURST_ERROR_RESPONSE`          |
| Non-null `byte_enable_ptr`                      | `b_transport` precheck   | `TLM_BYTE_ENABLE_ERROR_RESPONSE`    |
| `address ≥ size_bytes` or `address + len > size_bytes` | `b_transport` window check | `TLM_ADDRESS_ERROR_RESPONSE`     |
| Unsupported command (`TLM_IGNORE_COMMAND`)      | `b_transport` switch     | `TLM_COMMAND_ERROR_RESPONSE`        |
| Construction with `size_bytes = 0` or non-multiple of 8 | Constructor      | `SC_REPORT_FATAL` at elaboration    |
| Construction with `access_delay_ns < 0`         | Constructor              | `SC_REPORT_FATAL` at elaboration    |
| Preload file missing / unreadable               | `load_preload()`         | `SC_REPORT_FATAL` at elaboration    |
| Malformed hex line                              | `parse_hex_line()`       | `SC_REPORT_FATAL` (file + line no.) |
| Hex preload overflow with **non-zero** data     | `load_preload()`         | `SC_REPORT_FATAL`                   |
| Hex preload overflow with **all-zero** tail     | `load_preload()`         | Tolerated (truncated silently)      |
| In-window write                                  | `b_transport`            | `TLM_OK_RESPONSE`, contents unchanged (claim C4) |

`SC_REPORT_FATAL` is reserved for elaboration-time misconfiguration;
runtime failures use TLM response codes so the surrounding fabric and
test bench see deterministic bus-level errors rather than simulator
aborts.

---

## 12. Preload formats

### 12.1 Hex format (`init_file_format = "hex"`)

Plain text, one 64-bit word per line, big-endian ASCII hex (most
significant nibble first). Compatible with `xxd -r -p` round-tripping.

```
# Lines starting with '#' are comments.
# Blank lines are ignored.
000005177c105073      # word 0 — written to data_[0..7] LSB-first
3055107301450513      # word 1 — written to data_[8..15] LSB-first
```

- Each line yields one 8-byte chunk stored starting at the next
  consecutive 8-byte slot (word 0 → offset 0, word 1 → offset 8, …).
- The 64-bit value is stored **little-endian** in `data_`, matching
  the on-disk layout of the binary format below (so the same image
  produces identical in-memory bytes regardless of which preload format
  was used).
- Optional `0x` / `0X` prefix is accepted.
- Trailing `# comment` text is stripped.

### 12.2 Binary format (`init_file_format = "bin"`)

Raw binary, dumped to disk as a contiguous byte stream. Each 8 bytes
on disk form one 64-bit word; bytes are stored at the matching offset
in `data_` without any swap. Matches the `xxd` / `dd` / `objcopy
-O binary` conventions used by the SMC tooling for `.img` artefacts.

### 12.3 `auto` resolution

The default `init_file_format = "auto"` picks `bin` if the filename
ends in `.img` or `.bin` (case-insensitive), and `hex` otherwise. Any
other value falls back to the same auto-detection rule, but the
`SC_REPORT_INFO` banner emitted at construction time shows the
resolved format so the operator always knows which path was taken.

### 12.4 Zero-padding rules

| File size      | Preload result                                                |
|----------------|---------------------------------------------------------------|
| 0 bytes        | `SC_REPORT_FATAL` (empty preload is treated as an error)      |
| `< size_bytes` | Loaded into `data_[0..N-1]`; the remainder is zero            |
| `= size_bytes` | Exact fit                                                     |
| `> size_bytes` (hex) | Tolerated **iff** every overflow word is zero; non-zero overflow word → `SC_REPORT_FATAL` |
| `> size_bytes` (bin) | Truncated silently (binary streams may be padded for tooling) |

---

## 13. Compliance matrix

| Claim | Source                                  | Statement                                                  | Model behaviour |
|-------|-----------------------------------------|------------------------------------------------------------|-----------------|
| C2    | `test_sep_boot_rom_rw.py::test_read_after_reset` | `rvalid` asserts 1 cycle after a read request                | `b_transport` annotates `access_delay_ns` (default 1 ns) |
| C3    | `test_sep_boot_rom_rw.py::test_preload_readback` | Reads return preloaded data (or zero if not preloaded)       | Reads return `data_[off..off+len-1]` verbatim |
| C4    | `test_sep_boot_rom_rw.py::test_rom_write_ignored` | Writes silently ignored; `rvalid` still generated            | `b_transport` returns `TLM_OK_RESPONSE`; `data_` unchanged |
| —     | `test_sep_boot_rom_preload.py::test_preload_zero_init` | Without preload, ROM words are zero                        | `init_file = ""` → `data_.assign(size, 0)` |
| —     | `test_sep_boot_rom_preload.py::test_preload_first_words` | First 16 words of preload file match `data_`               | Hex / binary parsers both verified by test bench |
| —     | RDL `sw=r, hw=r`                         | ROM is software-readonly                                    | Production + debug writes both silently discard data |
| —     | RDL `memwidth=64`                        | 64-bit native word width                                    | Default `size_bytes` is `NUM_ENTRIES × 8`; 8-byte access is the maximum |
| —     | SMC IP convention                        | All SMC IPs expose `rst_n_i` even when no state is reset    | `rst_n_i` registered as `SC_METHOD`; body is intentionally empty |

### 13.1 Documented deviations

None. All behaviour matches the RDL and the cocotb conformance suite.

### 13.2 Unmodelled features

| Item                                  | Reason                                                                |
|---------------------------------------|-----------------------------------------------------------------------|
| Approximately-Timed (AT) protocol     | LT model is sufficient for SW bring-up.                               |
| Per-word programmable read latency    | Production RTL always returns 1-cycle `rvalid`; modelling sub-word    |
|                                       | latency variation would be misleading at the transaction level.       |
| DMI fast-path                         | Kept parked behind the same convention as PLIC/CLINT; trivial to add. |
| Secure-region / key-disable lock      | Not part of the current RDL; future revisions may add a CCI flag.     |

---

## 14. Revision history

| Version | Date       | Author        | Notes                                                                                           |
|---------|------------|---------------|-------------------------------------------------------------------------------------------------|
| 1.0     | 2026-05-26 | SMC modelling | Initial release — matches `sep_boot_rom.rdl` and the SEP cocotb conformance suite (C2/C3/C4).   |

---

## 15. Glossary

- **Boot ROM** — A small, preloaded, read-only memory containing the
  first instructions executed by a processor after reset.
- **Preload** — The act of populating ROM contents at elaboration time
  from an external file (hex or binary).
- **rvalid** — The AXI4 read-data-valid handshake; the LT model
  abstracts this into the annotated `b_transport` delay.
- **Sparse-non-zero overflow** — A preload file longer than the target
  ROM whose overflow region is not entirely zero; treated as a
  configuration error to prevent silent image-size mismatches.
- **LT (Loosely-Timed)** — TLM-2.0 modelling style: `b_transport`,
  optional DMI, coarse `sc_time` annotations.
- **RDL** — SystemRDL register-description language; the authoritative
  Boot ROM declaration lives in
  `tt-oca-hw/meta/registers/rdl/sep_boot_rom.rdl`.
- **CCI** — SystemC Configuration, Control and Inspection (Accellera
  cci-1.0.0). Provides the `cci_param<T>` interface used here.
- **SC_REPORT_FATAL** — SystemC's mechanism for reporting an
  unrecoverable configuration error during elaboration.

---

*End of document.*
