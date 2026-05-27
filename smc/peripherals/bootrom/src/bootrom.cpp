// SPDX-License-Identifier: Apache-2.0
/**
 * @file bootrom.cpp
 * @brief SEP Boot ROM — SystemC/TLM-2.0 LT implementation.
 *
 * See `include/bootrom.h` for the full design description, memory map,
 * conformance contract, and preload formats.
 */

#include "bootrom.h"

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

/// Return the all-lowercase copy of @p s; used for case-insensitive
/// suffix matching (".HEX", ".Hex", etc.).
std::string to_lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

/// True iff @p s ends with @p suffix (case-insensitive).
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

bootrom::bootrom(sc_core::sc_module_name name, bootrom_cfg cfg)
    : sc_core::sc_module(name)
    // CCI params constructed first (declared before public ports in bootrom.h).
    // The cfg.* values become DEFAULTS; broker presets override them.
    , size_bytes_p_(
          "size_bytes",
          cfg.size_bytes,
          "Total ROM size in bytes. Must be a non-zero multiple of 8 "
          "(native 64-bit word width per sep_boot_rom.rdl).")
    , init_file_p_(
          "init_file",
          cfg.init_file,
          "Path to the preload image. Empty = zero-initialised ROM "
          "(matches the test_preload_zero_init conformance claim).")
    , init_file_format_p_(
          "init_file_format",
          cfg.init_file_format,
          "Preload-image format. 'hex' = one 64-bit big-endian ASCII word "
          "per line; 'bin' = raw little-endian binary; 'auto' picks 'bin' "
          "for .img/.bin filenames, otherwise 'hex'.")
    , access_delay_ns_p_(
          "access_delay_ns",
          cfg.access_delay_ns,
          "TLM register-access annotated delay in nanoseconds. "
          "Conformance claim C2 requires rvalid 1 cycle after the read "
          "request; 1 ns matches a 1 GHz fabric clock. Mutable at run time.")
    , reg_socket("reg_socket")
    , rst_n_i   ("rst_n_i")
    , cfg_(cfg)
{
    // Sync cfg_ with the (possibly preset-overridden) CCI values so the
    // rest of the constructor sees consistent data.
    cfg_.size_bytes       = size_bytes_p_.get_value();
    cfg_.init_file        = init_file_p_.get_value();
    cfg_.init_file_format = init_file_format_p_.get_value();
    cfg_.access_delay_ns  = access_delay_ns_p_.get_value();

    // Provenance metadata — visible to introspection tools and inspectors.
    size_bytes_p_.add_metadata("rdl_dimension",
                               cci::cci_value(std::string("mem_array[NUM_ENTRIES]")));
    size_bytes_p_.add_metadata("default_KiB", cci::cci_value(unsigned(64)));
    init_file_p_.add_metadata("plusarg_equivalent",
                              cci::cci_value(std::string("+sep_boot_rom_preload=<file>")));
    init_file_format_p_.add_metadata("valid_values",
                                     cci::cci_value(std::string("hex|bin|auto")));
    access_delay_ns_p_.add_metadata("unit",      cci::cci_value(std::string("nanoseconds")));
    access_delay_ns_p_.add_metadata("tlm_phase", cci::cci_value(std::string("annotated_delay")));

    // ----------------------------------------------------------------------
    // Configuration validation
    //
    // We fail loud at elaboration time (SC_REPORT_FATAL) rather than at
    // first transaction — a misconfigured ROM size or unreadable preload
    // image is a platform-integration bug, not a run-time error.
    // ----------------------------------------------------------------------
    if (cfg_.size_bytes == 0) {
        SC_REPORT_FATAL(name, "bootrom size_bytes must be non-zero");
    }
    if ((cfg_.size_bytes % bootrom_cfg::WORD_BYTES) != 0) {
        SC_REPORT_FATAL(name,
            "bootrom size_bytes must be a multiple of 8 (native word width)");
    }
    if (cfg_.access_delay_ns < 0.0) {
        SC_REPORT_FATAL(name, "bootrom access_delay_ns must be >= 0");
    }

    // ----------------------------------------------------------------------
    // Allocate and preload contents.
    //
    // We always allocate the full `size_bytes` even if the preload file is
    // smaller — the tail is zero-initialised, exactly as the cocotb
    // `test_preload_zero_init` test expects.
    // ----------------------------------------------------------------------
    data_.assign(cfg_.size_bytes, 0);
    if (!cfg_.init_file.empty()) {
        load_preload(data_);
    }

    // Cache the time-typed CCI value.  Re-cached on every b_transport
    // so a CCI-driven mutation takes effect on the next access.
    access_delay_ = sc_core::sc_time(cfg_.access_delay_ns, sc_core::SC_NS);

    // TLM target socket callbacks.
    reg_socket.register_b_transport  (this, &bootrom::b_transport);
    reg_socket.register_transport_dbg(this, &bootrom::transport_dbg);

    // SC_METHOD: reset on rst_n_i value change.  A ROM has no mutable
    // state, so this is a logging-only handler — but we register it so
    // that the model behaves identically to its sister IPs and so any
    // future state added here gets a natural reset path.
    SC_METHOD(reset_proc);
    sensitive << rst_n_i;
    dont_initialize();

    SC_REPORT_INFO(name,
        ("bootrom instantiated: size_bytes=" + std::to_string(cfg_.size_bytes) +
         ", init_file=\"" + cfg_.init_file + "\"" +
         ", init_file_format=" + resolve_format(cfg_.init_file_format, cfg_.init_file) +
         ", access_delay_ns=" + std::to_string(cfg_.access_delay_ns)).c_str());
}

// ---------------------------------------------------------------------------
// SC_METHOD: reset_proc
// ---------------------------------------------------------------------------
//
// The Boot ROM has no mutable state to clear on reset.  We still expose an
// SC_METHOD on `rst_n_i` so that the model:
//   1. behaves like every other SMC IP from the platform integrator's POV
//      (binding rst_n_i is a no-op rather than a runtime warning), and
//   2. has an obvious extension point if a future revision adds, e.g.,
//      a programmable boot-region select or a read-disable lock.

void bootrom::reset_proc()
{
    // Intentionally empty body.  Reads through `b_transport` are always
    // served from the preloaded `data_` regardless of reset state, which
    // is exactly how the RTL behaves (the cocotb conformance suite reads
    // immediately after `reset()` and expects valid data).
}

// ---------------------------------------------------------------------------
// b_transport — the only blocking TLM entry point
// ---------------------------------------------------------------------------
//
// 1. Validate length / alignment / streaming-width
// 2. Reject byte-enables and unsupported commands
// 3. Reads: return data_[addr..addr+len-1]
// 4. Writes: silently ignored (per the C4 conformance claim)
// 5. Annotate `delay` with `access_delay_ns_p_` (re-cached every txn)

void bootrom::b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay)
{
    const sc_dt::uint64 addr   = gp.get_address();
    const unsigned      length = gp.get_data_length();
    unsigned char* const buf   = gp.get_data_ptr();

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
    if (gp.get_byte_enable_ptr() != nullptr &&
        gp.get_byte_enable_length() != 0) {
        gp.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        return;
    }

    // ---- Window check -----------------------------------------------------
    if (addr >= cfg_.size_bytes || addr + length > cfg_.size_bytes) {
        gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    if (gp.is_read()) {
        std::memcpy(buf, data_.data() + addr, length);
    } else if (gp.is_write()) {
        // C4: silently discard writes; do NOT mutate data_.  The bus
        // contract requires rvalid (TLM_OK_RESPONSE) to assert anyway.
    } else {
        gp.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    // Re-cache access_delay each transaction so a CCI-driven change takes
    // effect on the *next* access (the conversion is two adds — cheap).
    access_delay_ = sc_core::sc_time(access_delay_ns_p_.get_value(), sc_core::SC_NS);
    delay += access_delay_;
    gp.set_response_status(tlm::TLM_OK_RESPONSE);
    gp.set_dmi_allowed(false);
}

// ---------------------------------------------------------------------------
// transport_dbg — back-door read/write, no annotated delay
// ---------------------------------------------------------------------------
//
// Symmetric to `b_transport` for the validation path; writes are silently
// ignored just like the production path.  Returns the number of bytes
// transferred, or 0 on validation error (per TLM-2.0 convention).

unsigned int bootrom::transport_dbg(tlm::tlm_generic_payload& gp)
{
    const sc_dt::uint64 addr   = gp.get_address();
    const unsigned      length = gp.get_data_length();
    unsigned char* const buf   = gp.get_data_ptr();

    if (!is_supported_width(length)) return 0;
    if (!is_aligned(addr, length))   return 0;
    if (addr >= cfg_.size_bytes || addr + length > cfg_.size_bytes) return 0;

    if (gp.is_read()) {
        std::memcpy(buf, data_.data() + addr, length);
    }
    // Debug writes are silently ignored (same contract as b_transport).
    return length;
}

// ---------------------------------------------------------------------------
// Debug back-door API
// ---------------------------------------------------------------------------

uint64_t bootrom::dbg_read64(uint64_t off) const
{
    if (off + 8 > cfg_.size_bytes || (off & 7u) != 0) return 0;
    uint64_t v;
    std::memcpy(&v, data_.data() + off, 8);
    return v;
}

uint32_t bootrom::dbg_read32(uint64_t off) const
{
    if (off + 4 > cfg_.size_bytes || (off & 3u) != 0) return 0;
    uint32_t v;
    std::memcpy(&v, data_.data() + off, 4);
    return v;
}

unsigned bootrom::dbg_load_bytes(uint64_t off, const uint8_t* ptr, unsigned len)
{
    if (ptr == nullptr || off >= cfg_.size_bytes) return 0;
    const unsigned writable = static_cast<unsigned>(
        std::min<uint64_t>(len, cfg_.size_bytes - off));
    std::memcpy(data_.data() + off, ptr, writable);
    return writable;
}

void bootrom::dump_state(std::ostream& os) const
{
    os << "bootrom state @ " << sc_core::sc_time_stamp() << "\n"
       << "  size_bytes       = 0x" << std::hex << cfg_.size_bytes
       << " (" << std::dec << cfg_.size_bytes << ")\n"
       << "  init_file        = \"" << cfg_.init_file << "\"\n"
       << "  init_file_format = " << resolve_format(cfg_.init_file_format,
                                                    cfg_.init_file) << "\n"
       << "  access_delay_ns  = " << cfg_.access_delay_ns << "\n";

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
// Preload helpers
// ---------------------------------------------------------------------------

std::string bootrom::resolve_format(const std::string& format,
                                    const std::string& path)
{
    if (format == "hex" || format == "bin") return format;
    if (format != "auto") {
        // Fall through to auto-detection so a typo doesn't kill the run;
        // the SC_REPORT_FATAL in `load_preload` will catch true errors.
    }
    if (ends_with_ci(path, ".img") || ends_with_ci(path, ".bin")) return "bin";
    return "hex";
}

bool bootrom::parse_hex_line(const std::string& raw,
                             uint64_t& value, bool& consumed)
{
    consumed = false;
    // Strip leading whitespace
    auto it = raw.begin();
    while (it != raw.end() && std::isspace(static_cast<unsigned char>(*it))) ++it;
    if (it == raw.end() || *it == '#') return true; // blank / comment line

    // Strip optional 0x / 0X prefix; the hex tools used by the SEP team
    // emit bare hex (no prefix) but we accept both for robustness.
    std::string s(it, raw.end());
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s.erase(0, 2);
    }
    // Strip trailing whitespace / comments
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

void bootrom::load_preload(std::vector<uint8_t>& data) const
{
    const std::string format = resolve_format(cfg_.init_file_format,
                                              cfg_.init_file);

    std::ifstream f;
    if (format == "bin") {
        f.open(cfg_.init_file, std::ios::binary);
        if (!f.is_open()) {
            SC_REPORT_FATAL(name(),
                ("bootrom: cannot open binary preload '" +
                 cfg_.init_file + "'").c_str());
        }
        // Stream up to `size_bytes` bytes; any tail is left zero.
        f.read(reinterpret_cast<char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
        const std::streamsize n = f.gcount();
        if (n <= 0) {
            SC_REPORT_FATAL(name(),
                ("bootrom: preload '" + cfg_.init_file +
                 "' is empty or unreadable").c_str());
        }
        return;
    }

    // ---- HEX path ---------------------------------------------------------
    f.open(cfg_.init_file);
    if (!f.is_open()) {
        SC_REPORT_FATAL(name(),
            ("bootrom: cannot open hex preload '" + cfg_.init_file + "'").c_str());
    }

    std::string line;
    unsigned    word_idx = 0;
    unsigned    line_no  = 0;
    while (std::getline(f, line)) {
        ++line_no;
        uint64_t v       = 0;
        bool     consumed = false;
        if (!parse_hex_line(line, v, consumed)) {
            std::ostringstream err;
            err << "bootrom: malformed hex line " << line_no
                << " in '" << cfg_.init_file << "': \"" << line << "\"";
            SC_REPORT_FATAL(name(), err.str().c_str());
        }
        if (!consumed) continue;

        const uint64_t off = uint64_t(word_idx) * bootrom_cfg::WORD_BYTES;
        if (off + bootrom_cfg::WORD_BYTES > data.size()) {
            // Tolerate a longer file iff the overflow tail is entirely zero.
            // Many hex generators emit a full-size memory image (e.g. the SMC
            // tooling produces 128 KiB even for a 64 KiB target ROM, with the
            // unused tail zero-padded).  Treating that as fatal would break
            // real-world preload artefacts.  A non-zero overflow word is
            // still rejected — that is genuinely a configuration mistake.
            if (v != 0) {
                std::ostringstream err;
                err << "bootrom: preload '" << cfg_.init_file
                    << "' line " << line_no
                    << " contains non-zero data at word " << word_idx
                    << " (offset 0x" << std::hex << off << std::dec
                    << ") but ROM size is " << data.size() << " B";
                SC_REPORT_FATAL(name(), err.str().c_str());
            }
            ++word_idx;
            continue;
        }
        std::memcpy(data.data() + off, &v, bootrom_cfg::WORD_BYTES);
        ++word_idx;
    }

    if (word_idx == 0) {
        SC_REPORT_FATAL(name(),
            ("bootrom: hex preload '" + cfg_.init_file +
             "' contains no data lines").c_str());
    }
}

} // namespace smc
