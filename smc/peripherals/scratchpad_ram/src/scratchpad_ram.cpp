// SPDX-License-Identifier: Apache-2.0
/**
 * @file scratchpad_ram.cpp
 * @brief SMC Scratchpad RAM — SystemC/TLM-2.0 LT implementation.
 *
 * See `include/scratchpad_ram.h` for the full design description, memory map,
 * behaviour contract, SECDED ECC model, and preload formats.
 */

#include "scratchpad_ram.h"

#include "sim_log.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace smc {

namespace {

/// Return the all-lowercase copy of @p s; used for case-insensitive suffix
/// matching (".HEX", ".Hex", etc.).
std::string to_lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

/// True iff @p s ends with @p suffix (case-insensitive).
///
/// Used by resolve_format() to pick the preload format from the file name when
/// `init_file_format == "auto"`: a `.img` / `.bin` suffix (in any letter case,
/// e.g. ".IMG", ".Bin") selects the raw-binary loader, otherwise the hex
/// loader is used.
bool ends_with_ci(const std::string& s, const std::string& suffix)
{
    if (suffix.size() > s.size()) return false;
    return to_lower(s.substr(s.size() - suffix.size())) == to_lower(suffix);
}

/// True iff @p len is one of the natively-supported access widths.
constexpr bool is_supported_width(unsigned len)
{
    return len == 1 || len == 2 || len == 4 || len == 8;
}

/// True iff @p addr is naturally aligned to @p len bytes.
constexpr bool is_aligned(uint64_t addr, unsigned len)
{
    return (addr & (uint64_t(len) - 1)) == 0;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

scratchpad_ram::scratchpad_ram(sc_core::sc_module_name name,
                               scratchpad_ram_cfg cfg)
    : sc_core::sc_module(name)
    // CCI params constructed first (declared before public ports in the
    // header).  The cfg.* values become DEFAULTS; broker presets override them.
    , size_bytes_p_(
          "size_bytes",
          cfg.size_bytes,
          "Total scratchpad RAM size in bytes. Must be a non-zero multiple of "
          "8 (native 64-bit word width per spm_memory.rdl).")
    , init_file_p_(
          "init_file",
          cfg.init_file,
          "Optional path to a preload image. Empty = zero-initialised RAM "
          "(the usual cold-boot state).")
    , init_file_format_p_(
          "init_file_format",
          cfg.init_file_format,
          "Preload-image format. 'hex' = one 64-bit big-endian ASCII word per "
          "line; 'bin' = raw little-endian binary; 'auto' picks 'bin' for "
          ".img/.bin filenames, otherwise 'hex'.")
    , access_delay_ns_p_(
          "access_delay_ns",
          cfg.access_delay_ns,
          "TLM b_transport annotated delay in nanoseconds. The RTL scratchpad "
          "pipelines its read path (~2 cycles); 2 ns matches a 1 GHz fabric "
          "clock. Mutable at run time.")
    , ecc_enabled_p_(
          "ecc_enabled",
          cfg.ecc_enabled,
          "Whether SECDED ECC behaviour (error injection + single-bit scrub) "
          "is exposed. The production scratchpad is SECDED-protected.")
    , reg_socket("reg_socket")
    , rst_n_i   ("rst_n_i")
    , cfg_(cfg)
{
    // Sync cfg_ with the (possibly preset-overridden) CCI values so the rest
    // of the constructor sees consistent data.
    cfg_.size_bytes       = size_bytes_p_.get_value();
    cfg_.init_file        = init_file_p_.get_value();
    cfg_.init_file_format = init_file_format_p_.get_value();
    cfg_.access_delay_ns  = access_delay_ns_p_.get_value();
    cfg_.ecc_enabled      = ecc_enabled_p_.get_value();

    // Provenance metadata — visible to introspection tools and inspectors.
    size_bytes_p_.add_metadata("rdl_dimension",
                               cci::cci_value(std::string("mem_array[NUM_ENTRIES]")));
    size_bytes_p_.add_metadata("default_KiB", cci::cci_value(unsigned(64)));
    size_bytes_p_.add_metadata("rtl_source",
                               cci::cci_value(std::string("WithScratchpadWithECC(size=0x10000)")));
    init_file_format_p_.add_metadata("valid_values",
                                     cci::cci_value(std::string("hex|bin|auto")));
    access_delay_ns_p_.add_metadata("unit",      cci::cci_value(std::string("nanoseconds")));
    access_delay_ns_p_.add_metadata("tlm_phase", cci::cci_value(std::string("annotated_delay")));
    ecc_enabled_p_.add_metadata("ecc_code", cci::cci_value(std::string("SECDED")));

    // ----------------------------------------------------------------------
    // Configuration validation.  Fail loud at elaboration time
    // (SC_REPORT_FATAL) rather than at first transaction — a misconfigured
    // RAM size or unreadable preload image is a platform-integration bug.
    // ----------------------------------------------------------------------
    if (cfg_.size_bytes == 0) {
        SC_REPORT_FATAL(name, "scratchpad_ram size_bytes must be non-zero");
    }
    if ((cfg_.size_bytes % scratchpad_ram_cfg::WORD_BYTES) != 0) {
        SC_REPORT_FATAL(name,
            "scratchpad_ram size_bytes must be a multiple of 8 (native word width)");
    }
    if (cfg_.access_delay_ns < 0.0) {
        SC_REPORT_FATAL(name, "scratchpad_ram access_delay_ns must be >= 0");
    }

    // ----------------------------------------------------------------------
    // Allocate and (optionally) preload contents.  The full `size_bytes` is
    // always allocated; the tail (or the whole RAM when no preload is given)
    // is zero-initialised, matching cold-boot SRAM state.
    // ----------------------------------------------------------------------
    data_.assign(cfg_.size_bytes, 0);
    if (!cfg_.init_file.empty()) {
        load_preload(data_);
    }

    // Cache the time-typed CCI value.  Re-cached on every b_transport so a
    // CCI-driven mutation takes effect on the next access.
    access_delay_ = sc_core::sc_time(cfg_.access_delay_ns, sc_core::SC_NS);

    // TLM target socket callbacks.
    reg_socket.register_b_transport  (this, &scratchpad_ram::b_transport);
    reg_socket.register_transport_dbg(this, &scratchpad_ram::transport_dbg);

    // SC_METHOD: reset on rst_n_i value change.  SRAM retains its contents, so
    // this is a logging-only handler — registered so the model behaves
    // identically to its sister IPs and so any future state added here gets a
    // natural reset path.
    SC_METHOD(reset_proc);
    sensitive << rst_n_i;
    dont_initialize();

    SIM_LOG_INFO(this,
        "scratchpad_ram instantiated: size_bytes=" << cfg_.size_bytes
        << ", init_file=\"" << cfg_.init_file << "\""
        << ", init_file_format=" << resolve_format(cfg_.init_file_format, cfg_.init_file)
        << ", access_delay_ns=" << cfg_.access_delay_ns
        << ", ecc_enabled=" << (cfg_.ecc_enabled ? "true" : "false"));
}

// ---------------------------------------------------------------------------
// SC_METHOD: reset_proc
// ---------------------------------------------------------------------------
//
// The scratchpad RAM retains its contents across a logical reset — the RTL
// only resets the TileLink bus pipeline registers (d_full / r_full), not the
// SRAM array.  We still expose an SC_METHOD on `rst_n_i` so the model binds
// the same way as every other SMC IP and has an obvious extension point.

void scratchpad_ram::reset_proc()
{
    // Intentionally empty body.  SRAM contents survive reset; reads through
    // `b_transport` continue to return stored data throughout the reset window.
}

// ---------------------------------------------------------------------------
// ECC helpers
// ---------------------------------------------------------------------------

int scratchpad_ram::ecc_check(uint64_t off, unsigned len, bool scrub)
{
    if (!cfg_.ecc_enabled || ecc_errors_.empty()) return 0;

    const uint64_t word_bytes = scratchpad_ram_cfg::WORD_BYTES;
    const uint64_t first = (off / word_bytes) * word_bytes;
    const uint64_t last  = ((off + len - 1) / word_bytes) * word_bytes;

    int status = 0; // 0 clean, 1 correctable, 2 uncorrectable
    for (uint64_t w = first; w <= last; w += word_bytes) {
        auto it = ecc_errors_.find(w);
        if (it == ecc_errors_.end()) continue;
        if (it->second) {
            // Uncorrectable dominates; report immediately.
            return 2;
        }
        status = std::max(status, 1);
        if (scrub) {
            // SECDED hardware corrects single-bit errors in place on access.
            ecc_errors_.erase(it);
        }
    }
    return status;
}

void scratchpad_ram::ecc_clear_range(uint64_t off, unsigned len)
{
    if (ecc_errors_.empty()) return;
    const uint64_t word_bytes = scratchpad_ram_cfg::WORD_BYTES;
    const uint64_t first = (off / word_bytes) * word_bytes;
    const uint64_t last  = ((off + len - 1) / word_bytes) * word_bytes;
    for (uint64_t w = first; w <= last; w += word_bytes) {
        ecc_errors_.erase(w);
    }
}

// ---------------------------------------------------------------------------
// b_transport — the only blocking TLM entry point
// ---------------------------------------------------------------------------
//
// 1. Validate length / alignment / streaming-width
// 2. Window check
// 3. ECC check (reads of an uncorrectable word fail; correctable is scrubbed)
// 4. Reads: copy data_[addr..]; honour byte-enables
// 5. Writes: commit to data_; honour byte-enables; re-encode ECC (clear flags)
// 6. Annotate `delay` with `access_delay_ns_p_` (re-cached every txn)

void scratchpad_ram::b_transport(tlm::tlm_generic_payload& gp,
                                 sc_core::sc_time& delay)
{
    const sc_dt::uint64 addr   = gp.get_address();
    const unsigned      length = gp.get_data_length();
    unsigned char* const buf   = gp.get_data_ptr();
    const unsigned char* const be = gp.get_byte_enable_ptr();
    const unsigned      be_len = gp.get_byte_enable_length();

    // ---- Width / alignment / streaming-width validation -------------------
    if (!is_supported_width(length)) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (!is_aligned(addr, length)) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (gp.get_streaming_width() != length) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }

    // ---- Window check -----------------------------------------------------
    if (addr >= cfg_.size_bytes || addr + length > cfg_.size_bytes) {
        gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    if (gp.is_read()) {
        // SECDED: an uncorrectable word fails the read (TileLink d_corrupt);
        // a correctable word is repaired transparently and scrubbed.
        if (ecc_check(addr, length, /*scrub=*/true) == 2) {
            gp.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
        if (be != nullptr && be_len != 0) {
            for (unsigned i = 0; i < length; ++i) {
                if (be[i % be_len] == TLM_BYTE_ENABLED) {
                    buf[i] = data_[addr + i];
                }
            }
        } else {
            std::memcpy(buf, data_.data() + addr, length);
        }
        SIM_LOG_TRACE(this, "read  addr=0x" << std::hex << addr
                            << " len=" << std::dec << length);
    } else if (gp.is_write()) {
        // True RAM: writes commit.  Byte-enables select which bytes change
        // (the RTL implements this via a read-modify-write under the SECDED
        // code).  A full / partial write re-encodes ECC, clearing any error
        // previously injected on the touched words.
        if (be != nullptr && be_len != 0) {
            for (unsigned i = 0; i < length; ++i) {
                if (be[i % be_len] == TLM_BYTE_ENABLED) {
                    data_[addr + i] = buf[i];
                }
            }
        } else {
            std::memcpy(data_.data() + addr, buf, length);
        }
        ecc_clear_range(addr, length);
        SIM_LOG_TRACE(this, "write addr=0x" << std::hex << addr
                            << " len=" << std::dec << length);
    } else {
        gp.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    // Re-cache access_delay each transaction so a CCI-driven change takes
    // effect on the next access.
    access_delay_ = sc_core::sc_time(access_delay_ns_p_.get_value(), sc_core::SC_NS);
    delay += access_delay_;
    gp.set_response_status(tlm::TLM_OK_RESPONSE);
    gp.set_dmi_allowed(false);
}

// ---------------------------------------------------------------------------
// transport_dbg — back-door read/write, no annotated delay, ECC-bypass
// ---------------------------------------------------------------------------
//
// Symmetric to `b_transport` for the validation path, but bypasses ECC
// (debugger access).  Returns the number of bytes transferred, or 0 on
// validation error (per TLM-2.0 convention).

unsigned int scratchpad_ram::transport_dbg(tlm::tlm_generic_payload& gp)
{
    const sc_dt::uint64 addr   = gp.get_address();
    const unsigned      length = gp.get_data_length();
    unsigned char* const buf   = gp.get_data_ptr();

    if (!is_supported_width(length)) return 0;
    if (!is_aligned(addr, length))   return 0;
    if (addr >= cfg_.size_bytes || addr + length > cfg_.size_bytes) return 0;

    if (gp.is_read()) {
        std::memcpy(buf, data_.data() + addr, length);
    } else if (gp.is_write()) {
        std::memcpy(data_.data() + addr, buf, length);
        ecc_clear_range(addr, length);
    }
    return length;
}

// ---------------------------------------------------------------------------
// Debug back-door API
// ---------------------------------------------------------------------------

uint64_t scratchpad_ram::dbg_read64(uint64_t off) const
{
    if (off + 8 > cfg_.size_bytes || (off & 7u) != 0) return 0;
    uint64_t v;
    std::memcpy(&v, data_.data() + off, 8);
    return v;
}

uint32_t scratchpad_ram::dbg_read32(uint64_t off) const
{
    if (off + 4 > cfg_.size_bytes || (off & 3u) != 0) return 0;
    uint32_t v;
    std::memcpy(&v, data_.data() + off, 4);
    return v;
}

unsigned scratchpad_ram::dbg_load_bytes(uint64_t off, const uint8_t* ptr,
                                        unsigned len)
{
    if (ptr == nullptr || off >= cfg_.size_bytes) return 0;
    const unsigned writable = static_cast<unsigned>(
        std::min<uint64_t>(len, cfg_.size_bytes - off));
    std::memcpy(data_.data() + off, ptr, writable);
    ecc_clear_range(off, writable);
    return writable;
}

void scratchpad_ram::dbg_inject_ecc_error(uint64_t off, bool correctable)
{
    if (!cfg_.ecc_enabled || off >= cfg_.size_bytes) return;
    const uint64_t word = (off / scratchpad_ram_cfg::WORD_BYTES) *
                          scratchpad_ram_cfg::WORD_BYTES;
    ecc_errors_[word] = !correctable; // value == uncorrectable flag
}

void scratchpad_ram::dbg_clear_ecc_errors()
{
    ecc_errors_.clear();
}

void scratchpad_ram::dump_state(std::ostream& os) const
{
    os << "scratchpad_ram state @ " << sc_core::sc_time_stamp() << "\n"
       << "  size_bytes       = 0x" << std::hex << cfg_.size_bytes
       << " (" << std::dec << cfg_.size_bytes << ")\n"
       << "  init_file        = \"" << cfg_.init_file << "\"\n"
       << "  init_file_format = " << resolve_format(cfg_.init_file_format,
                                                    cfg_.init_file) << "\n"
       << "  access_delay_ns  = " << cfg_.access_delay_ns << "\n"
       << "  ecc_enabled      = " << (cfg_.ecc_enabled ? "true" : "false")
       << "  (injected errors: " << ecc_errors_.size() << ")\n";

    const unsigned preview = std::min<unsigned>(32, data_.size());
    os << "  contents[0.." << preview - 1 << "] =";
    for (unsigned i = 0; i < preview; ++i) {
        if ((i & 7u) == 0) os << "\n    ";
        os << " " << std::hex << std::setw(2) << std::setfill('0')
           << unsigned(data_[i]);
    }
    os << std::dec << std::setfill(' ') << "\n";
}

// ---------------------------------------------------------------------------
// Preload helpers (identical format to the Boot ROM's preload pipeline)
// ---------------------------------------------------------------------------

std::string scratchpad_ram::resolve_format(const std::string& format,
                                           const std::string& path)
{
    if (format == "hex" || format == "bin") return format;
    // Any other value (including "auto" or a typo) falls through to
    // suffix-based auto-detection; the SC_REPORT_FATAL in load_preload still
    // catches genuine file errors.
    if (ends_with_ci(path, ".img") || ends_with_ci(path, ".bin")) return "bin";
    return "hex";
}

bool scratchpad_ram::parse_hex_line(const std::string& raw,
                                    uint64_t& value, bool& consumed)
{
    consumed = false;
    auto it = raw.begin();
    while (it != raw.end() && std::isspace(static_cast<unsigned char>(*it))) ++it;
    if (it == raw.end() || *it == '#') return true; // blank / comment line

    std::string s(it, raw.end());
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s.erase(0, 2);
    }
    auto comment = s.find('#');
    if (comment != std::string::npos) s.erase(comment);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    if (s.empty()) return true;

    try {
        size_t pos = 0;
        value = std::stoull(s, &pos, 16);
        if (pos != s.size()) return false; // trailing junk
    } catch (const std::exception&) {
        return false;
    }
    consumed = true;
    return true;
}

void scratchpad_ram::load_preload(std::vector<uint8_t>& data) const
{
    const std::string format = resolve_format(cfg_.init_file_format,
                                              cfg_.init_file);

    std::ifstream f;
    if (format == "bin") {
        f.open(cfg_.init_file, std::ios::binary);
        if (!f.is_open()) {
            SC_REPORT_FATAL(name(),
                ("scratchpad_ram: cannot open binary preload '" +
                 cfg_.init_file + "'").c_str());
        }
        f.read(reinterpret_cast<char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
        const std::streamsize n = f.gcount();
        if (n <= 0) {
            SC_REPORT_FATAL(name(),
                ("scratchpad_ram: preload '" + cfg_.init_file +
                 "' is empty or unreadable").c_str());
        }
        return;
    }

    // ---- HEX path ---------------------------------------------------------
    f.open(cfg_.init_file);
    if (!f.is_open()) {
        SC_REPORT_FATAL(name(),
            ("scratchpad_ram: cannot open hex preload '" +
             cfg_.init_file + "'").c_str());
    }

    std::string line;
    unsigned    word_idx = 0;
    unsigned    line_no  = 0;
    while (std::getline(f, line)) {
        ++line_no;
        uint64_t v        = 0;
        bool     consumed = false;
        if (!parse_hex_line(line, v, consumed)) {
            std::ostringstream err;
            err << "scratchpad_ram: malformed hex line " << line_no
                << " in '" << cfg_.init_file << "': \"" << line << "\"";
            SC_REPORT_FATAL(name(), err.str().c_str());
        }
        if (!consumed) continue;

        const uint64_t off = uint64_t(word_idx) * scratchpad_ram_cfg::WORD_BYTES;
        if (off + scratchpad_ram_cfg::WORD_BYTES > data.size()) {
            // Tolerate a longer file iff the overflow tail is entirely zero
            // (hex generators often emit a full-size image with the unused
            // tail zero-padded).  A non-zero overflow word is a real mistake.
            if (v != 0) {
                std::ostringstream err;
                err << "scratchpad_ram: preload '" << cfg_.init_file
                    << "' line " << line_no
                    << " contains non-zero data at word " << word_idx
                    << " (offset 0x" << std::hex << off << std::dec
                    << ") but RAM size is " << data.size() << " B";
                SC_REPORT_FATAL(name(), err.str().c_str());
            }
            ++word_idx;
            continue;
        }
        std::memcpy(data.data() + off, &v, scratchpad_ram_cfg::WORD_BYTES);
        ++word_idx;
    }

    if (word_idx == 0) {
        SC_REPORT_FATAL(name(),
            ("scratchpad_ram: hex preload '" + cfg_.init_file +
             "' contains no data lines").c_str());
    }
}

} // namespace smc
