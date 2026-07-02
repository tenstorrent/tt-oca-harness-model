# 02 — sep_status_report Low-Level Design

## Components

```
StatusDecoder (include/sep_status_decoder.h)   pure C++, no SystemC
    on_word(uint32_t)        split type/fw_id/value, format line, emit via callback
    on_bytes(data,len)       little-endian assemble (ignore len<4 / null) -> on_word
    set_names(map)           install value->name lookup
    parse_tsv(istream)       static: TSV -> value->name map (skip comments/blank/malformed)
    set_enabled(bool)        gate emission

SepStatusReport (include/sep_status_report.h, src/sep_status_report.cpp)  sc_module
    CsmlLogger + csml_param<int> verbosity, <bool> enable, <string> names_tsv
    ctor: configure logger format "[...] [SEP_STATUS] - %MESSAGE%", load TSV, wire emit
    on_word / on_bytes       forward to StatusDecoder
    enabled()                read back the enable param (platform uses it to gate the tap)
```

## Decode

`type = (w>>24)&0xFF`, `fw_id = (w>>16)&0xFF`, `value = w&0xFFFF`. Labels via small
switches; unknown type → `T0x%02x`, unknown fw-id → `ID%u`. Line:
`snprintf("%-3s %-8s 0x%04x %s", stage, severity, value, name)`. Name via map lookup,
miss → `SEP_MSG_UNKNOWN`.

## TSV parser

Line classification (after trimming): blank → skip; inside `/* */` → skip (track open/close);
line starting `/*` → enter block (unless it also closes); line starting `//` or `*` → skip;
otherwise split into two whitespace-separated columns `(name, value)`; `value` parsed as
hex (`std::stoul(.., 16)`, whole token consumed); insert `value16 → name`. Any row that
fails these steps is skipped. Pure/static — unit-tested from in-memory strings.

## Platform integration (vp/platform/sep/och_sep_ss.hpp)

- Instance `sep_status` created in `create_modules()`.
- Ring placement is expressed once as shared `constexpr` (`STATUS_RING_LOCAL`,
  `STATUS_RING_DATA`, `STATUS_RING_DATA_END`, `STATUS_RING_ENTRIES`) used by both the
  SMC-handshake seed block and the tap; seeded `num_entries` is 512.
- In `module_bind()`, when `sep_status->enabled()`, an observation-only write-tap is
  installed on the `smc_global` `SEPMemory` instance:

  ```
  if (off >= STATUS_RING_DATA && off < STATUS_RING_DATA_END && len >= 4 && (off % 4) == 0)
      sep_status->on_bytes(off, data, len);
  ```

  The window filter excludes the `head`/`tail`/`num_entries` header writes and the
  construction-time seeding (which uses `load_data`, not `b_transport`). The ISS routes
  accesses through `b_transport` (DMI disabled when a tap is installed), so every entry
  write is observed.

## Emission & verbosity

Lines are emitted with `CSML_INFO(2, logger)` so they show at the platform default
verbosity (firmware status output, peer to UART/`SIM_OUT`). TSV load problems are reported
once with `CSML_WARN`.

## Determinism

The TSV is parsed once at construction; decode is a pure function of the word and the map.
Given a fixed firmware image, configuration, and TSV, output is identical across runs.
