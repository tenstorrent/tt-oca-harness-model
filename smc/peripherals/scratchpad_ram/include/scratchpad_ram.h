// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file scratchpad_ram.h
 * @brief SystemC/TLM-2.0 Loosely-Timed (LT) model of the SMC Scratchpad RAM.
 *
 * This module is a **loosely-timed, transaction-level model** of the
 * on-chip scratchpad RAM instantiated in the System Management Controller
 * (SMC) CPU cluster of the OCA hardware platform.  It is intended for
 * software bring-up, integration testing, and early firmware development —
 * not for micro-architectural timing analysis.
 *
 * Unlike the Boot ROM (a read-only memory), the scratchpad is a true
 * **read-write SRAM**: the SMC Rocket cores use it for early-boot stack and
 * data storage before external DRAM is trained, and as a fast local scratch
 * region thereafter.
 *
 * ---
 * ## Authoritative references
 *
 * | Source | Role |
 * |--------|------|
 * | `tt-oca-hw/hw/smc/smc_cpu/data/registers/rdl/spm_memory.rdl` | **Ground-truth memory declaration** (`mem`, 64-bit, `sw=rw, hw=rw`) |
 * | `tt-oca-hw/hw/smc/smc_cpu/chipyard_config/OCAH1CORECluster.scala` | `WithScratchpadWith​ECC(base=0xC0040000, size=0x10000, banks=1, partitions=4, SECDED)` |
 * | `tt-oca-hw/hw/smc/smc_cpu/chipyard_generated_files/1core/OCAH1CORECluster_TLRAM.sv` | Generated TileLink SRAM RTL (byte-mask writes, SECDED, RMW) |
 * | `tt-oca-hw/hw/smc/data/registers/rdl/smc_top.rdl` | SMC top-level address map (`spm_memory @ BASE_ADDR + 0x06_0000`) |
 * | `01_BOOTROM_Specification.md`, `01_PLIC_Specification.md` | Sister SMC IPs — same modelling conventions |
 *
 * ---
 * ## Memory map (from spm_memory.rdl / chipyard scratchpad config)
 *
 * ```
 * Offset            Size      SW    Description
 * ──────────────────────────────────────────────────────────────────────────
 * 0x00000           8B × N    RW    mem_array[i] = mem_word { data[63:0] }
 *                                   Default: 64 KiB (matches the generated
 *                                   1-core WithScratchpadWithECC size=0x10000).
 *                                   `sw=rw, hw=rw` (true read-write SRAM).
 *
 * >= size_bytes  (out)               Out-of-window → TLM_ADDRESS_ERROR_RESPONSE.
 * ```
 *
 * The RTL TileLink SRAM is 64-bit wide with an 8-bit byte mask, so the model
 * accepts every naturally-aligned access in `{1, 2, 4, 8}` bytes and honours
 * per-byte byte-enables (the RTL implements sub-word writes via a
 * read-modify-write so the SECDED code stays consistent over the full word).
 *
 * ---
 * ## Behaviour contract
 *
 * | Property            | Behaviour |
 * |---------------------|-----------|
 * | Reads               | Return `data_[off..off+len-1]`; honour byte-enables. |
 * | Writes              | Commit to `data_`; honour byte-enables (partial-word writes). |
 * | Read-after-write    | Returns the just-written value (true RAM, unlike the ROM). |
 * | Reset               | Structural no-op — SRAM retains contents across logical reset (only the RTL bus pipeline registers reset). |
 * | Out-of-window       | `TLM_ADDRESS_ERROR_RESPONSE`. |
 * | Misaligned / bad width / streaming mismatch | `TLM_BURST_ERROR_RESPONSE`. |
 * | Uncorrectable ECC (injected) | `TLM_GENERIC_ERROR_RESPONSE` (models the TileLink `d_corrupt` bit). |
 *
 * ---
 * ## SECDED ECC model
 *
 * The RTL scratchpad protects each 64-bit word with a SECDED (single-error-
 * correct, double-error-detect) code.  A bit-accurate ECC model is out of
 * scope for an LT functional model; instead the model offers a **behavioural
 * abstraction** controlled by the `ecc_enabled` CCI parameter and a debug
 * error-injection API:
 *
 * - `dbg_inject_ecc_error(off, correctable=true)` marks a word as having a
 *   single-bit error.  The next read **corrects** it transparently (data is
 *   returned and the flag is scrubbed), matching SECDED hardware.
 * - `dbg_inject_ecc_error(off, correctable=false)` marks a word as having
 *   an uncorrectable (double-bit) error.  Reads overlapping it return
 *   `TLM_GENERIC_ERROR_RESPONSE` (the LT analogue of the TileLink `d_corrupt`
 *   bit).  Writing the whole word clears the error (the RMW path re-encodes
 *   fresh ECC).
 *
 * With no injected errors (the default) the model behaves as a plain RAM.
 *
 * ---
 * ## CCI configuration parameters
 *
 * | Name              | Type        | Default     | Mutability | Purpose |
 * |-------------------|-------------|-------------|------------|---------|
 * | `size_bytes`      | `uint64_t`  | `0x10000`   | immutable  | Total RAM size (must be a non-zero multiple of 8). |
 * | `init_file`       | `std::string` | `""`      | immutable  | Optional preload image; empty = zero-fill. |
 * | `init_file_format`| `std::string` | `"auto"`  | immutable  | `"hex"`, `"bin"`, or `"auto"`. |
 * | `access_delay_ns` | `double`    | `2.0`       | mutable    | TLM `b_transport` annotated delay (pipelined ~2-cycle read). |
 * | `ecc_enabled`     | `bool`      | `true`      | immutable  | Enables the SECDED error-injection / scrub behaviour. |
 *
 * Override before construction via the CCI broker:
 *
 * @code
 *   broker.set_preset_cci_value("…scratchpad_ram.size_bytes",
 *                               cci::cci_value(uint64_t(0x10000)));
 *   broker.set_preset_cci_value("…scratchpad_ram.access_delay_ns",
 *                               cci::cci_value(2.0));
 * @endcode
 */

#ifndef SMC_SCRATCHPAD_RAM_H_
#define SMC_SCRATCHPAD_RAM_H_

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <cci_configuration>

#include "smc_axi_extension.h"

namespace smc {

// ---------------------------------------------------------------------------
// scratchpad_ram_cfg
// ---------------------------------------------------------------------------

/**
 * @brief Compile-time defaults and structural constants for the SMC Scratchpad RAM.
 *
 * `size_bytes`, `init_file`, `init_file_format`, `access_delay_ns`, and
 * `ecc_enabled` may be overridden via CCI presets; the values here are the
 * defaults used when no preset is supplied.
 */
struct scratchpad_ram_cfg {
    /// Total RAM size, in bytes.  Must be a non-zero multiple of 8 (the
    /// native 64-bit word width).  Default 64 KiB matches the generated
    /// 1-core `WithScratchpadWithECC(size = 0x10000)` configuration.
    uint64_t size_bytes = 0x10000ULL;

    /// Optional preload-image path (hex or binary).  Empty string means
    /// "zero-initialise the RAM" — the usual cold-boot state.
    std::string init_file{};

    /// Preload file format: `"hex"`, `"bin"`, or `"auto"`.
    ///   - `auto`: pick `bin` if the filename ends `.img` / `.bin`,
    ///             otherwise `hex`.
    std::string init_file_format = "auto";

    /// TLM `b_transport` annotated delay in nanoseconds.  The RTL scratchpad
    /// pipelines its read path (~2 cycles), so 2 ns at a 1 GHz fabric clock
    /// is the LT default.
    double access_delay_ns = 2.0;

    /// Whether the model exposes SECDED ECC behaviour (error injection +
    /// single-bit scrub).  Always true for the production scratchpad; can be
    /// disabled to model a non-ECC RAM.
    bool ecc_enabled = true;

    /// Native RAM word width in bytes (informational; fixed by the RDL
    /// `memwidth = 64`).  Also the ECC-protection granule.
    static constexpr unsigned WORD_BYTES = 8;

    /// Largest TLM access width accepted in a single `b_transport`.
    static constexpr unsigned MAX_ACCESS_BYTES = 8;
};

// ---------------------------------------------------------------------------
// scratchpad_ram
// ---------------------------------------------------------------------------

/**
 * @brief SystemC/TLM-2.0 LT model of the SMC Scratchpad RAM.  CCI-parameterised.
 *
 * ### Ports
 *
 * | Port         | Direction | Width | Description |
 * |--------------|-----------|-------|-------------|
 * | `reg_socket` | target    | —     | TLM-2.0 target socket; AXI4 / TileLink-style read-write access |
 * | `rst_n_i`    | input     | 1b    | Active-low synchronous reset (no-op for SRAM contents; kept for IP-suite symmetry) |
 *
 * The scratchpad's contents are **mutable run-time state** (unlike the Boot
 * ROM).  Reset does not clear them — real SRAM retains its data across a
 * logical reset, and the RTL only resets the TileLink bus pipeline registers.
 *
 * ### Internal SC processes
 *
 * | Process      | Sensitivity | Purpose |
 * |--------------|-------------|---------|
 * | `reset_proc` | `rst_n_i`   | Logs reset events; no SRAM state cleared. |
 *
 * ### TLM-2.0 interface notes
 *
 * - Accepts naturally-aligned `1`, `2`, `4`, or `8`-byte accesses.
 * - Reads return the byte slice `data_[addr..addr+len-1]`.
 * - Writes commit to `data_`; **byte-enables are honoured** (the RTL supports
 *   per-byte masks via a read-modify-write), distinguishing this model from
 *   the byte-enable-rejecting Boot ROM.
 * - Out-of-window addresses (≥ `size_bytes`) → `TLM_ADDRESS_ERROR_RESPONSE`.
 * - Misaligned addresses, unsupported widths, or `streaming_width` mismatches
 *   → `TLM_BURST_ERROR_RESPONSE`.
 * - Reads of a word with an injected uncorrectable ECC error →
 *   `TLM_GENERIC_ERROR_RESPONSE`.
 * - DMI is **not** registered.  A RAM is a strong DMI candidate, but a DMI
 *   write would bypass the SECDED re-encode path, so we keep it un-granted for
 *   parity with the rest of the SMC IPs (see the low-level design future-work
 *   item).
 * - `transport_dbg` provides side-effect-free back-door reads and writes that
 *   bypass ECC, matching debugger access semantics.
 *
 * ### Unmodelled RTL features
 *
 * - **Atomic memory operations (AMOs).**  The TileLink SRAM supports
 *   arithmetic / logical atomics via its read-modify-write ALU.  The LT model
 *   treats writes as plain stores; AMO support is a documented future-work
 *   item (see the low-level design).
 * - **Bit-accurate SECDED.**  Modelled behaviourally via the injection API
 *   rather than a true Hamming code.
 */
class scratchpad_ram : public sc_core::sc_module {
protected:
    // ------------------------------------------------------------------
    // CCI configuration parameters.
    //
    // Declared *before* the public ports so they are constructed first.
    // size_bytes is read by the constructor to size the internal data_
    // vector.  size_bytes / init_file / init_file_format / ecc_enabled are
    // immutable; access_delay_ns is mutable and re-read on every transaction.
    // ------------------------------------------------------------------

    /// Total RAM size in bytes (must be non-zero and a multiple of 8).
    cci::cci_param<uint64_t, cci::CCI_IMMUTABLE_PARAM> size_bytes_p_;

    /// Optional preload-image path; empty → zero-initialise.
    cci::cci_param<std::string, cci::CCI_IMMUTABLE_PARAM> init_file_p_;

    /// Preload-image format: "hex", "bin", or "auto".
    cci::cci_param<std::string, cci::CCI_IMMUTABLE_PARAM> init_file_format_p_;

    /// TLM `b_transport` annotated delay (ns).  Mutable.
    cci::cci_param<double> access_delay_ns_p_;

    /// Whether SECDED ECC behaviour is exposed.
    cci::cci_param<bool, cci::CCI_IMMUTABLE_PARAM> ecc_enabled_p_;

public:
    SC_HAS_PROCESS(scratchpad_ram);

    /// TLM-2.0 target socket for read-write access.  Connect any AXI /
    /// AXI-Lite / TileLink initiator (typically the SMC CPU cluster) here.
    tlm_utils::simple_target_socket<scratchpad_ram> reg_socket;

    /// Active-low synchronous reset.  Does not clear SRAM contents — kept for
    /// SMC IP-suite symmetry.
    sc_core::sc_in<bool> rst_n_i;

    /**
     * @brief Construct the Scratchpad RAM module.
     * @param name  SystemC module name.
     * @param cfg   Sizing / preload defaults.  CCI presets (if any) take
     *              priority over these values.
     */
    explicit scratchpad_ram(sc_core::sc_module_name name,
                            scratchpad_ram_cfg cfg = scratchpad_ram_cfg{});

    // ------------------------------------------------------------------
    // Debug back-door API (no bus delay, ECC-bypass)
    // ------------------------------------------------------------------

    /// Read an aligned 64-bit word (back-door, no bus delay, ignores ECC).
    /// Returns 0 if `off` is out of range or unaligned.
    uint64_t dbg_read64(uint64_t off) const;

    /// Read an aligned 32-bit word (back-door, no bus delay, ignores ECC).
    uint32_t dbg_read32(uint64_t off) const;

    /// Copy @p len bytes from @p ptr into the RAM at offset @p off (back-door).
    /// Writing a word clears any ECC error flag on that word.  Returns the
    /// number of bytes actually written (0 if @p off is OOB).
    unsigned dbg_load_bytes(uint64_t off, const uint8_t* ptr, unsigned len);

    /// Inject a SECDED ECC error on the 64-bit word containing @p off.
    /// `correctable == true`  → single-bit error (scrubbed on next read);
    /// `correctable == false` → uncorrectable error (reads fail until the
    /// word is overwritten).  No-op if `ecc_enabled` is false or @p off is OOB.
    void dbg_inject_ecc_error(uint64_t off, bool correctable);

    /// Clear all injected ECC error flags.
    void dbg_clear_ecc_errors();

    /// Total RAM size in bytes (CCI-resolved value).
    uint64_t size_bytes() const { return cfg_.size_bytes; }

    /// Whether SECDED ECC behaviour is enabled (CCI-resolved value).
    bool ecc_enabled() const { return cfg_.ecc_enabled; }

    /// Print a human-readable snapshot of the RAM configuration and the first
    /// 32 bytes of contents to @p os.
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

    /// Active-low reset handler.  No-op aside from a single `SC_REPORT_INFO`
    /// on assertion (SRAM retains its contents).
    void reset_proc();

    // ------------------------------------------------------------------
    // ECC helpers
    // ------------------------------------------------------------------

    /// Return the ECC status of the access [off, off+len): 0 = clean,
    /// 1 = a covered word has a correctable error (scrub it), 2 = a covered
    /// word has an uncorrectable error.  Scrubbing of correctable errors is a
    /// side effect of a *read*; callers that only validate pass `scrub=false`.
    int ecc_check(uint64_t off, unsigned len, bool scrub);

    /// Drop ECC error flags on every word overlapping [off, off+len) — used
    /// when a write re-encodes those words.
    void ecc_clear_range(uint64_t off, unsigned len);

    // ------------------------------------------------------------------
    // Preload helpers (shared format with the Boot ROM)
    // ------------------------------------------------------------------

    /// Resolve the effective format ("hex" or "bin") given a user-supplied
    /// `init_file_format` (which may be "auto") and the filename's suffix.
    static std::string resolve_format(const std::string& format,
                                      const std::string& path);

    /// Load the preload image into @p data.  On failure, issues
    /// `SC_REPORT_FATAL` with a descriptive message.
    void load_preload(std::vector<uint8_t>& data) const;

    /// Parse a single hex line: returns false if the line is invalid (so the
    /// caller can report the file/line number).  Blank / comment-only lines
    /// yield `true` with `consumed = false`.
    static bool parse_hex_line(const std::string& line,
                               uint64_t& value, bool& consumed);

    // ------------------------------------------------------------------
    // State
    // ------------------------------------------------------------------

    scratchpad_ram_cfg   cfg_;       ///< Resolved configuration (from CCI).
    std::vector<uint8_t> data_;      ///< RAM contents (size = cfg_.size_bytes).
    sc_core::sc_time     access_delay_; ///< Cached value of access_delay_ns_p_.

    /// Sparse map of injected ECC errors: key = word-aligned byte offset,
    /// value = true for uncorrectable, false for correctable.  Empty unless
    /// errors have been injected for testing.
    std::unordered_map<uint64_t, bool> ecc_errors_;
};

} // namespace smc

#endif // SMC_SCRATCHPAD_RAM_H_
