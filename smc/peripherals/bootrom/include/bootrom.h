// SPDX-License-Identifier: Apache-2.0
/**
 * @file bootrom.h
 * @brief SystemC/TLM-2.0 Loosely-Timed (LT) model of the SEP Boot ROM.
 *
 * This module is a **loosely-timed, transaction-level model** of the
 * boot ROM instantiated in the SEP (Secure Entry Processor) sub-system of
 * the OCA hardware platform.  It is intended for software bring-up,
 * integration testing, and early firmware development — not for
 * micro-architectural timing analysis.
 *
 * The Boot ROM is functionally trivial — a preloaded, read-only memory —
 * but it is the **very first** target the SEP core accesses after reset,
 * which makes it a critical bring-up dependency.  Getting the preload
 * format, address aliasing, and write-ignore semantics exactly right is
 * what makes firmware boot at all in the SystemC platform.
 *
 * ---
 * ## Authoritative references
 *
 * | Source | Role |
 * |--------|------|
 * | `tt-oca-hw/meta/registers/rdl/sep_boot_rom.rdl` | **Ground-truth memory map** |
 * | `tt-oca-hw/dv/oss/shims/sep/memories/tests/conformance/test_sep_boot_rom_rw.py`      | Conformance contract C2–C4 (read after reset, write-ignore) |
 * | `tt-oca-hw/dv/oss/shims/sep/memories/tests/conformance/test_sep_boot_rom_preload.py` | Preload format (hex / binary, plusarg semantics) |
 * | `tt-oca-hw/hw/smc/data/scripts/bootrom.rv64.img`     | Sample binary preload image |
 * | `tt-oca-hw/dv/smc/tb/meta/scripts/bootrom_sanity.rv64.hex` | Sample hex preload image |
 * | `01_PLIC_Specification.md`, `01_CLINT_Specification.md` | Sister SMC IPs — same modelling conventions |
 *
 * ---
 * ## Memory map (from sep_boot_rom.rdl)
 *
 * ```
 * Offset            Size      SW    Description
 * ──────────────────────────────────────────────────────────────────────────
 * 0x00000           8B × N    R     mem_array[i] = mem_word { data[63:0] }
 *                                   Default: N = 0x2000 (64 KiB total).
 *                                   `sw=r, hw=r` (true ROM).
 *
 * 0x10000…  (out)                   Out-of-window → TLM_ADDRESS_ERROR_RESPONSE.
 * ```
 *
 * The RDL declares `regwidth = memwidth = 64`, but the SEP fabric and the
 * Rocket cores routinely issue 1-, 2-, 4-, and 8-byte aligned accesses
 * (RV64 instruction fetch is 4 B; data loads can be byte-, half-, word-,
 * or doubleword-wide).  The model therefore accepts every naturally-
 * aligned access in `{1, 2, 4, 8}` bytes.
 *
 * ---
 * ## Conformance contract
 *
 * Distilled from the cocotb conformance suite in
 * `dv/oss/shims/sep/memories/tests/conformance/`:
 *
 * | Claim | Statement                                              | Model behaviour |
 * |-------|--------------------------------------------------------|-----------------|
 * | C2    | A read returns valid data 1 cycle after the request     | `b_transport` annotates `access_delay_ns` (default 1 ns). |
 * | C3    | Reads return preloaded data (or zero if not preloaded) | `data_[off..off+len-1]` returned verbatim. |
 * | C4    | Write request is silently ignored; rvalid still asserts | `b_transport` returns `TLM_OK_RESPONSE` and leaves `data_` unchanged. |
 *
 * Out-of-window addresses and misaligned / unsupported-width accesses
 * return TLM bus errors, matching the SMC IP convention (see PLIC §11).
 *
 * ---
 * ## Preload formats
 *
 * Two file formats are supported, selected via the `init_file_format` CCI
 * parameter (default `"auto"`):
 *
 * - **`hex`** — text, one 64-bit word per line in big-endian ASCII hex
 *   (i.e. `MSB ... LSB`).  Blank lines and `#`-prefixed comments are
 *   ignored.  Each line is stored at the next consecutive 8-byte slot.
 *   Matches `dv/smc/tb/meta/scripts/bootrom_sanity.rv64.hex`.
 *
 * - **`bin`** — raw binary, little-endian, dumped consecutively starting
 *   at offset 0.  Truncated to `size_bytes` if the file is larger; padded
 *   with zeros if smaller.  Matches `hw/smc/data/scripts/bootrom.rv64.img`.
 *
 * - **`auto`** (default) — pick `bin` if the filename ends in `.img` or
 *   `.bin`, otherwise `hex`.
 *
 * If `init_file` is empty the ROM is zero-initialised, matching the
 * cocotb `test_preload_zero_init` claim.
 *
 * ---
 * ## CCI configuration parameters
 *
 * | Name                | Type        | Default     | Mutability | Purpose |
 * |---------------------|-------------|-------------|------------|---------|
 * | `size_bytes`        | `uint64_t`  | `0x10000`   | immutable  | Total ROM size (must be a multiple of 8). |
 * | `init_file`         | `std::string` | `""`      | immutable  | Path to preload image; empty = zero-fill. |
 * | `init_file_format`  | `std::string` | `"auto"`  | immutable  | `"hex"`, `"bin"`, or `"auto"`. |
 * | `access_delay_ns`   | `double`    | `1.0`       | mutable    | TLM `b_transport` annotated delay. |
 *
 * Override before construction via the CCI broker:
 *
 * @code
 *   broker.set_preset_cci_value("…bootrom.size_bytes",
 *                               cci::cci_value(uint64_t(0x20000)));
 *   broker.set_preset_cci_value("…bootrom.init_file",
 *                               cci::cci_value(std::string("bootrom.hex")));
 *   broker.set_preset_cci_value("…bootrom.access_delay_ns",
 *                               cci::cci_value(5.0));
 * @endcode
 */

#ifndef SMC_BOOTROM_H_
#define SMC_BOOTROM_H_

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include <cci_configuration>

#include "smc_axi_extension.h"

namespace smc {

// ---------------------------------------------------------------------------
// bootrom_cfg
// ---------------------------------------------------------------------------

/**
 * @brief Compile-time defaults and address-map constants for the SEP Boot ROM.
 *
 * The address-map constants are fixed by `sep_boot_rom.rdl` and must not
 * be altered.  `size_bytes`, `init_file`, `init_file_format`, and
 * `access_delay_ns` may be overridden via CCI presets.
 */
struct bootrom_cfg {
    /// Total ROM size, in bytes.  Must be a non-zero multiple of 8 (the
    /// native 64-bit word width).  Default 64 KiB matches
    /// `sep_boot_rom.rdl NUM_ENTRIES = 0x2000`.
    uint64_t size_bytes = 0x10000ULL;

    /// Path to the preload image (hex or binary).  Empty string means
    /// "zero-initialise the ROM" (matches the `test_preload_zero_init`
    /// conformance claim).
    std::string init_file{};

    /// Preload file format: `"hex"`, `"bin"`, or `"auto"`.
    ///   - `auto`: pick `bin` if the filename ends `.img` / `.bin`,
    ///             otherwise `hex`.
    std::string init_file_format = "auto";

    /// TLM `b_transport` annotated delay in nanoseconds.  The conformance
    /// suite (C2) only requires `rvalid` one cycle after a read request;
    /// 1 ns matches a 1 GHz fabric clock.
    double access_delay_ns = 1.0;

    /// Native ROM word width in bytes (informational; fixed by the RDL).
    /// Used for preload chunking and the dump_state default formatter.
    static constexpr unsigned WORD_BYTES = 8;

    /// Largest TLM access width accepted in a single `b_transport`.
    /// 8 bytes matches the RDL `memwidth = 64` and is also a multiple of
    /// every smaller power-of-two access width.
    static constexpr unsigned MAX_ACCESS_BYTES = 8;
};

// ---------------------------------------------------------------------------
// bootrom
// ---------------------------------------------------------------------------

/**
 * @brief SystemC/TLM-2.0 LT model of the SEP Boot ROM.  CCI-parameterised.
 *
 * ### Ports
 *
 * | Port         | Direction | Width | Description |
 * |--------------|-----------|-------|-------------|
 * | `reg_socket` | target    | —     | TLM-2.0 target socket; AXI4-style read access |
 * | `rst_n_i`    | input     | 1b    | Active-low synchronous reset (no-op for a ROM; kept for IP-suite symmetry) |
 *
 * The Boot ROM contains **no mutable run-time state**: contents are fixed
 * at elaboration time from the preload image.  Reset is therefore a
 * structural no-op — but we still expose `rst_n_i` so that platform
 * top-level binders can wire it the same way they wire every other SMC IP.
 *
 * ### Internal SC processes
 *
 * | Process      | Sensitivity | Purpose |
 * |--------------|-------------|---------|
 * | `reset_proc` | `rst_n_i`   | Logs reset events; no state to clear. |
 *
 * (Unlike the CLINT, there is no recompute / output method — the ROM
 * has no outputs to drive.)
 *
 * ### TLM-2.0 interface notes
 *
 * - Accepts naturally-aligned `1`, `2`, `4`, or `8`-byte accesses.
 * - Reads return the byte slice `data_[addr..addr+len-1]`.
 * - Writes are **silently ignored** (the ROM is a pure memory), exactly
 *   as the cocotb `test_rom_write_ignored` and `test_rom_write_rvalid`
 *   conformance tests prescribe.  The response is still `TLM_OK_RESPONSE`
 *   so the fabric does not raise a bus error.
 * - Out-of-window addresses (≥ `size_bytes`) → `TLM_ADDRESS_ERROR_RESPONSE`.
 * - Misaligned addresses, unsupported widths, or `streaming_width` mismatches
 *   → `TLM_BURST_ERROR_RESPONSE`.
 * - Byte enables present → `TLM_BYTE_ENABLE_ERROR_RESPONSE` (matches CLINT).
 * - DMI is **not** registered.  ROM reads are perfect DMI candidates but we
 *   keep parity with the rest of the SMC IPs for now (see §16 of the
 *   low-level design for the future-work item).
 * - `transport_dbg` provides side-effect-free reads and (silently ignored)
 *   writes — same contract as `b_transport`, minus the annotated delay.
 */
class bootrom : public sc_core::sc_module {
protected:
    // ------------------------------------------------------------------
    // CCI configuration parameters.
    //
    // Declared *before* the public reg_socket / rst_n_i ports so that they
    // are constructed first.  size_bytes is read by the constructor to
    // size the internal data_ vector.
    //
    // size_bytes, init_file, init_file_format are immutable — changing the
    // ROM size or preload after elaboration would invalidate the in-memory
    // image and any cached pointers held by upstream initiators.
    // access_delay_ns is mutable and re-read on every transaction.
    // ------------------------------------------------------------------

    /// Total ROM size in bytes (must be non-zero and a multiple of 8).
    cci::cci_param<uint64_t, cci::CCI_IMMUTABLE_PARAM> size_bytes_p_;

    /// Preload-image path; empty → zero-initialise.
    cci::cci_param<std::string, cci::CCI_IMMUTABLE_PARAM> init_file_p_;

    /// Preload-image format: "hex", "bin", or "auto".
    cci::cci_param<std::string, cci::CCI_IMMUTABLE_PARAM> init_file_format_p_;

    /// TLM `b_transport` annotated delay (ns).  Mutable.
    cci::cci_param<double> access_delay_ns_p_;

public:
    SC_HAS_PROCESS(bootrom);

    /// TLM-2.0 target socket for read access.  Connect any AXI/AXI-Lite
    /// initiator (typically the SEP core fetch port) here.
    tlm_utils::simple_target_socket<bootrom> reg_socket;

    /// Active-low synchronous reset.  No internal state changes on
    /// assertion — kept for SMC IP-suite symmetry.
    sc_core::sc_in<bool> rst_n_i;

    /**
     * @brief Construct the Boot ROM module.
     * @param name  SystemC module name.
     * @param cfg   Sizing / preload defaults.  CCI presets (if any) take
     *              priority over these values.
     */
    explicit bootrom(sc_core::sc_module_name name,
                     bootrom_cfg cfg = bootrom_cfg{});

    // ------------------------------------------------------------------
    // Debug back-door API (const, no side effects)
    // ------------------------------------------------------------------

    /// Read an aligned 64-bit ROM word (back-door, no bus delay).
    /// Returns 0 if `off` is out of range or unaligned.
    uint64_t dbg_read64(uint64_t off) const;

    /// Read an aligned 32-bit ROM word (back-door, no bus delay).
    uint32_t dbg_read32(uint64_t off) const;

    /// Total ROM size in bytes (CCI-resolved value).
    uint64_t size_bytes() const { return cfg_.size_bytes; }

    /**
     * @brief Test-bench helper: copy @p len bytes from @p ptr into the ROM
     *        starting at offset @p off.
     *
     * Used by unit tests to construct deterministic in-memory images
     * without needing an on-disk file.  Has no SystemC effect — the ROM
     * has no outputs — so callers do not need to settle delta cycles.
     *
     * Returns the number of bytes actually written (0 if @p off is OOB,
     * else min(len, size_bytes - off)).
     */
    unsigned dbg_load_bytes(uint64_t off, const uint8_t* ptr, unsigned len);

    /// Print a human-readable snapshot of the ROM configuration and the
    /// first 32 bytes of contents to @p os.
    void dump_state(std::ostream& os = std::cout) const;

private:
    // ------------------------------------------------------------------
    // TLM-2.0 callbacks (registered with reg_socket in the constructor)
    // ------------------------------------------------------------------

    void         b_transport (tlm::tlm_generic_payload& gp, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& gp);

    // ------------------------------------------------------------------
    // SC_METHOD processes
    // ------------------------------------------------------------------

    /// Active-low reset handler.  No-op aside from a single
    /// `SC_REPORT_INFO` on assertion (the ROM has no mutable state).
    void reset_proc();

    // ------------------------------------------------------------------
    // Preload helpers
    // ------------------------------------------------------------------

    /// Resolve the effective format ("hex" or "bin") given a user-supplied
    /// `init_file_format` (which may be "auto") and the filename's suffix.
    static std::string resolve_format(const std::string& format,
                                      const std::string& path);

    /// Load the preload image into @p data.  On failure, issues
    /// `SC_REPORT_FATAL` with a descriptive message.
    void load_preload(std::vector<uint8_t>& data) const;

    /// Parse a single hex line: returns false if the line is invalid (so
    /// the caller can report the file/line number).  Blank / comment-only
    /// lines yield `true` with `consumed = false`.
    static bool parse_hex_line(const std::string& line,
                               uint64_t& value, bool& consumed);

    // ------------------------------------------------------------------
    // State
    // ------------------------------------------------------------------

    bootrom_cfg          cfg_;       ///< Resolved configuration (from CCI).
    std::vector<uint8_t> data_;      ///< ROM contents (size = cfg_.size_bytes).
    sc_core::sc_time     access_delay_; ///< Cached value of access_delay_ns_p_.
};

} // namespace smc

#endif // SMC_BOOTROM_H_
