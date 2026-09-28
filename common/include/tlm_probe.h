// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file tlm_probe.h
 * @brief Honest TLM-2.0 target probing for peripheral testbenches: a
 *        transport call that surrenders the response status and the annotated
 *        delay instead of discarding them, plus a reusable malformed-payload
 *        matrix.
 *
 * Why this exists
 * ---------------
 * The common testbench helper shape is to build a generic payload, set
 * `TLM_INCOMPLETE_RESPONSE`, call `b_transport()`, and return only the data
 * word. Two pieces of evidence are thrown away by that shape:
 *
 *   - **The response status.** A decode or protocol error leaves the read
 *     buffer at its initialised value, so a failed transaction is
 *     indistinguishable from a register that legitimately reads zero. Reset
 *     and read-only expectations are then satisfied by a broken transport.
 *     Helpers that additionally zero the caller's buffer on failure turn an
 *     error into a plausible-looking value.
 *   - **The annotated delay.** A target that forgets to annotate, or annotates
 *     the wrong amount, is invisible to a caller that overwrites or ignores
 *     the `sc_time` it passed in.
 *
 * So the rule here is that nothing is hidden and nothing is rewritten: every
 * access returns an ::simtlm::access_result carrying the status, the incoming
 * and outgoing delay, and the DMI hint. The caller decides what is expected.
 *
 * Deliberately policy-free
 * ------------------------
 * This header asserts nothing and prints nothing. SEP testbenches report
 * through a `check(bool, std::string)` member; SMC benches use local
 * `EXPECT_EQ` macros. Baking either one in would make the helper unusable by
 * the other half of the tree, so the result is returned and the existing
 * reporting mechanism stays in charge.
 *
 * Dependencies are TLM and SystemC only — no CCI, no register model, no
 * peripheral headers — so any testbench on either side of the tree can use it.
 * `common/include` is already on the include path for SEP and SMC peripherals,
 * so no build change is needed to adopt it.
 */
#pragma once

#include <tlm.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace simtlm {

/// Accellera exposes the byte-enable lane values as bare preprocessor macros
/// (`#define TLM_BYTE_ENABLED 0xff`), so they cannot be written `tlm::`-qualified
/// and they carry no type. Capture them once as typed constants.
inline constexpr unsigned char BYTE_ENABLED  = TLM_BYTE_ENABLED;
inline constexpr unsigned char BYTE_DISABLED = TLM_BYTE_DISABLED;

// ---------------------------------------------------------------------------
// Result of one transport call
// ---------------------------------------------------------------------------

/// Everything one `b_transport()` call revealed. Returned by value; the caller
/// asserts on whichever fields matter for the scenario under test.
struct access_result {
    tlm::tlm_response_status status    = tlm::TLM_INCOMPLETE_RESPONSE;
    sc_core::sc_time         delay_in  = sc_core::SC_ZERO_TIME;
    sc_core::sc_time         delay_out = sc_core::SC_ZERO_TIME;
    bool                     dmi_allowed = false;

    /// True only for an explicit `TLM_OK_RESPONSE`. Note that
    /// `TLM_INCOMPLETE_RESPONSE` is a failure here: a target that never touched
    /// the status left the transaction unhandled, which is itself a defect.
    bool ok() const { return status == tlm::TLM_OK_RESPONSE; }

    /// Delay the target added on top of what the initiator passed in. TLM-2.0
    /// requires targets to accumulate onto the incoming delay rather than
    /// overwrite it, so a correct target never returns less than it was given.
    sc_core::sc_time added_delay() const { return delay_out - delay_in; }

    /// True when the target overwrote rather than accumulated the delay. Worth
    /// asserting false whenever a non-zero incoming delay is supplied.
    bool delay_regressed() const { return delay_out < delay_in; }
};

/// Human-readable status, for assertion messages. `tlm_generic_payload` only
/// offers this as a member on a live payload, which is awkward once the
/// payload has gone out of scope.
inline const char* response_name(tlm::tlm_response_status s)
{
    switch (s) {
        case tlm::TLM_OK_RESPONSE:                return "TLM_OK_RESPONSE";
        case tlm::TLM_INCOMPLETE_RESPONSE:        return "TLM_INCOMPLETE_RESPONSE";
        case tlm::TLM_GENERIC_ERROR_RESPONSE:     return "TLM_GENERIC_ERROR_RESPONSE";
        case tlm::TLM_ADDRESS_ERROR_RESPONSE:     return "TLM_ADDRESS_ERROR_RESPONSE";
        case tlm::TLM_COMMAND_ERROR_RESPONSE:     return "TLM_COMMAND_ERROR_RESPONSE";
        case tlm::TLM_BURST_ERROR_RESPONSE:       return "TLM_BURST_ERROR_RESPONSE";
        case tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE: return "TLM_BYTE_ENABLE_ERROR_RESPONSE";
        default:                                  return "TLM_<unknown>";
    }
}

// ---------------------------------------------------------------------------
// Core access
// ---------------------------------------------------------------------------

/// Drive a fully caller-controlled payload and report what came back.
///
/// Templated on the socket type so it works with any
/// `tlm_utils::simple_initiator_socket<T>`, multi-socket, or raw
/// `tlm_fw_transport_if` pointer-like object that supports `sock->b_transport`.
///
/// The payload's response status is forced to `TLM_INCOMPLETE_RESPONSE` first,
/// so a target that silently ignores the transaction is reported as incomplete
/// rather than inheriting a stale OK from a reused payload.
template <typename Socket>
access_result access(Socket& sock,
                     tlm::tlm_generic_payload& gp,
                     sc_core::sc_time incoming = sc_core::SC_ZERO_TIME)
{
    access_result r;
    r.delay_in = incoming;

    sc_core::sc_time delay = incoming;
    gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    sock->b_transport(gp, delay);

    r.status      = gp.get_response_status();
    r.delay_out   = delay;
    r.dmi_allowed = gp.is_dmi_allowed();
    return r;
}

/// Well-formed read of one naturally aligned word.
///
/// @p value is written **only** when the access succeeds. On failure the
/// caller's variable is left untouched, so a test cannot mistake an error for
/// a zero read — the failure mode this header exists to remove.
template <typename Word, typename Socket>
access_result read_word(Socket& sock,
                        uint64_t address,
                        Word& value,
                        sc_core::sc_time incoming = sc_core::SC_ZERO_TIME)
{
    Word staging = Word{};
    tlm::tlm_generic_payload gp;
    gp.set_command(tlm::TLM_READ_COMMAND);
    gp.set_address(address);
    gp.set_data_ptr(reinterpret_cast<unsigned char*>(&staging));
    gp.set_data_length(sizeof(Word));
    gp.set_streaming_width(sizeof(Word));
    gp.set_byte_enable_ptr(nullptr);
    gp.set_byte_enable_length(0);
    gp.set_dmi_allowed(false);

    const access_result r = access(sock, gp, incoming);
    if (r.ok())
        value = staging;
    return r;
}

/// Read that exposes whatever the target did to the caller's buffer, including
/// on failure paths.
///
/// ::simtlm::read_word stages into a temporary and only commits on success,
/// which is what stops an error being mistaken for a zero. A negative test has
/// the opposite need: it asserts on the status *and* on what the target left
/// behind (some registers are specified to return zero data alongside an error
/// response). Use this only where the status is also being checked — otherwise
/// read_word() is the safer default.
template <typename Word, typename Socket>
access_result read_word_observed(Socket& sock,
                                 uint64_t address,
                                 Word& value,
                                 sc_core::sc_time incoming = sc_core::SC_ZERO_TIME)
{
    tlm::tlm_generic_payload gp;
    gp.set_command(tlm::TLM_READ_COMMAND);
    gp.set_address(address);
    gp.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    gp.set_data_length(sizeof(Word));
    gp.set_streaming_width(sizeof(Word));
    gp.set_byte_enable_ptr(nullptr);
    gp.set_byte_enable_length(0);
    gp.set_dmi_allowed(false);

    return access(sock, gp, incoming);
}

/// Well-formed write of one naturally aligned word.
template <typename Word, typename Socket>
access_result write_word(Socket& sock,
                         uint64_t address,
                         Word value,
                         sc_core::sc_time incoming = sc_core::SC_ZERO_TIME)
{
    tlm::tlm_generic_payload gp;
    gp.set_command(tlm::TLM_WRITE_COMMAND);
    gp.set_address(address);
    gp.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    gp.set_data_length(sizeof(Word));
    gp.set_streaming_width(sizeof(Word));
    gp.set_byte_enable_ptr(nullptr);
    gp.set_byte_enable_length(0);
    gp.set_dmi_allowed(false);

    return access(sock, gp, incoming);
}

/// Well-formed write carrying an explicit byte-enable array.
///
/// Distinct from the `byte_enable_*` entries in ::simtlm::defect, which ask
/// only whether the target gives a *defined* answer to an odd BE pattern. This
/// one is for targets that genuinely implement byte enables: it sends a legal
/// payload so the caller can assert the merge result lane by lane.
///
/// @p be_len shorter than `sizeof(Word)` is legal TLM — the array repeats
/// across the data — which is the case most byte-enable implementations get
/// wrong, so it is worth passing deliberately.
template <typename Word, typename Socket>
access_result write_word_be(Socket& sock,
                            uint64_t address,
                            Word value,
                            const unsigned char* be,
                            unsigned be_len,
                            sc_core::sc_time incoming = sc_core::SC_ZERO_TIME)
{
    tlm::tlm_generic_payload gp;
    gp.set_command(tlm::TLM_WRITE_COMMAND);
    gp.set_address(address);
    gp.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    gp.set_data_length(sizeof(Word));
    gp.set_streaming_width(sizeof(Word));
    // TLM's accessor is non-const; the target must not write through it.
    gp.set_byte_enable_ptr(const_cast<unsigned char*>(be));
    gp.set_byte_enable_length(be_len);
    gp.set_dmi_allowed(false);

    return access(sock, gp, incoming);
}

/// Debug transport. Returns the byte count the target claims to have
/// transferred; TLM-2.0 requires 0 on refusal and has no response status here.
template <typename Socket>
unsigned int debug_read(Socket& sock, uint64_t address,
                        unsigned char* buf, unsigned int len)
{
    tlm::tlm_generic_payload gp;
    gp.set_command(tlm::TLM_READ_COMMAND);
    gp.set_address(address);
    gp.set_data_ptr(buf);
    gp.set_data_length(len);
    gp.set_streaming_width(len);
    gp.set_byte_enable_ptr(nullptr);
    gp.set_byte_enable_length(0);
    return sock->transport_dbg(gp);
}

template <typename Socket>
unsigned int debug_write(Socket& sock, uint64_t address,
                         unsigned char* buf, unsigned int len)
{
    tlm::tlm_generic_payload gp;
    gp.set_command(tlm::TLM_WRITE_COMMAND);
    gp.set_address(address);
    gp.set_data_ptr(buf);
    gp.set_data_length(len);
    gp.set_streaming_width(len);
    gp.set_byte_enable_ptr(nullptr);
    gp.set_byte_enable_length(0);
    return sock->transport_dbg(gp);
}

/// Outcome of a DMI query, so a test can assert the policy rather than ignore it.
struct dmi_result {
    bool     granted = false;
    uint64_t start   = 0;
    uint64_t end     = 0;
    bool     read_allowed  = false;
    bool     write_allowed = false;
};

/// Ask a target for a direct memory pointer.
///
/// Most register models in this repo intend to refuse DMI. "Intend to" is the
/// problem: a refusal that is never asserted is indistinguishable from a target
/// that grants a window nobody noticed, so the plans ask for an explicit check.
template <typename Socket>
dmi_result dmi_request(Socket& sock, uint64_t address,
                       tlm::tlm_command cmd = tlm::TLM_READ_COMMAND)
{
    tlm::tlm_generic_payload gp;
    gp.set_command(cmd);
    gp.set_address(address);
    gp.set_data_ptr(nullptr);
    gp.set_data_length(0);
    gp.set_streaming_width(0);
    gp.set_byte_enable_ptr(nullptr);
    gp.set_byte_enable_length(0);

    tlm::tlm_dmi dmi;
    dmi_result out;
    out.granted = sock->get_direct_mem_ptr(gp, dmi);
    if (out.granted) {
        out.start         = dmi.get_start_address();
        out.end           = dmi.get_end_address();
        out.read_allowed  = dmi.is_read_allowed();
        out.write_allowed = dmi.is_write_allowed();
    }
    return out;
}

// ---------------------------------------------------------------------------
// Malformed-payload matrix
// ---------------------------------------------------------------------------

/// The generic-payload defects every target should have a defined answer for.
/// Each peripheral audit in `internal-review/testplans` lists some subset of
/// these as missing; this is their union so one traversal covers all of them.
enum class defect {
    well_formed,              ///< Control case: must succeed.
    null_data_ptr,            ///< Risk of a null dereference in the target.
    zero_length,
    short_length,             ///< 1 byte against a wider register.
    odd_length,               ///< 3 bytes: neither byte nor whole word.
    oversized_length,         ///< One byte past the natural width.
    unaligned_address,        ///< Natural width, offset by one byte.
    address_past_aperture,    ///< First address beyond the window.
    address_wrap,             ///< Near UINT64_MAX; exposes `addr + len` overflow.
    byte_enable_all_disabled,
    byte_enable_one_hot,      ///< Only lane 0 enabled.
    byte_enable_alternating,  ///< 0xff/0x00 pattern across the word.
    byte_enable_short,        ///< BE array shorter than data: must repeat.
    streaming_width_zero,
    streaming_width_partial,  ///< Non-zero but less than the data length.
    streaming_width_excess,   ///< Greater than the data length.
    ignore_command,           ///< Neither READ nor WRITE.
    stale_dmi_allowed,        ///< Incoming `dmi_allowed=true` must be cleared.
};

/// One row per enumerator, in declaration order. `defect_name` and
/// `all_defects` both walk this table so a new enumerator cannot be named in
/// one place and forgotten in the other. The values are contiguous from 0.
struct defect_row {
    defect      id;
    const char* name;
};

inline constexpr defect_row k_defects[] = {
    {defect::well_formed,              "well_formed"},
    {defect::null_data_ptr,            "null_data_ptr"},
    {defect::zero_length,              "zero_length"},
    {defect::short_length,             "short_length"},
    {defect::odd_length,               "odd_length"},
    {defect::oversized_length,         "oversized_length"},
    {defect::unaligned_address,        "unaligned_address"},
    {defect::address_past_aperture,    "address_past_aperture"},
    {defect::address_wrap,             "address_wrap"},
    {defect::byte_enable_all_disabled, "byte_enable_all_disabled"},
    {defect::byte_enable_one_hot,      "byte_enable_one_hot"},
    {defect::byte_enable_alternating,  "byte_enable_alternating"},
    {defect::byte_enable_short,        "byte_enable_short"},
    {defect::streaming_width_zero,     "streaming_width_zero"},
    {defect::streaming_width_partial,  "streaming_width_partial"},
    {defect::streaming_width_excess,   "streaming_width_excess"},
    {defect::ignore_command,           "ignore_command"},
    {defect::stale_dmi_allowed,        "stale_dmi_allowed"},
};

static_assert(static_cast<unsigned>(k_defects[0].id) == 0u,
              "defect enumerators must start at 0");
static_assert(static_cast<unsigned>(k_defects[std::size(k_defects) - 1].id) + 1u
                  == std::size(k_defects),
              "k_defects must list every defect enumerator exactly once");

inline const char* defect_name(defect d)
{
    const auto i = static_cast<unsigned>(d);
    if (i < std::size(k_defects) && k_defects[i].id == d)
        return k_defects[i].name;
    return "<unknown>";
}

/// What the probe needs to know about the target to build meaningful defects.
struct target_geometry {
    uint64_t valid_address = 0;   ///< An address known to decode successfully.
    unsigned word_bytes    = 4;   ///< Natural access width, typically 4 or 8.
    uint64_t aperture_bytes = 0;  ///< Window size; first invalid offset.
};

/// Every defect in ::simtlm::defect, in declaration order.
inline std::vector<defect> all_defects()
{
    std::vector<defect> out;
    out.reserve(std::size(k_defects));
    for (const defect_row& row : k_defects)
        out.push_back(row.id);
    return out;
}

/// A payload carrying one deliberate defect, owning its own data and
/// byte-enable buffers so the pointers stay valid for the transport call.
///
/// Non-copyable on purpose: copying would leave `gp` pointing into the
/// original's vectors. Build it, drive it, inspect it, let it die.
class malformed_payload {
public:
    malformed_payload(defect d, const target_geometry& geo,
                      tlm::tlm_command base_cmd = tlm::TLM_WRITE_COMMAND)
        : defect_(d)
    {
        const unsigned w = geo.word_bytes ? geo.word_bytes : 4;

        uint64_t     address = geo.valid_address;
        unsigned     length  = w;
        unsigned     swidth  = w;
        tlm::tlm_command cmd = base_cmd;
        bool         null_ptr = false;
        bool         dmi      = false;

        switch (d) {
            case defect::well_formed:
                break;
            case defect::null_data_ptr:
                null_ptr = true;
                break;
            case defect::zero_length:
                // Length is the only defect. Streaming width stays at the
                // natural word size; streaming_width_zero is its own case.
                length = 0;
                break;
            case defect::short_length:
                length = 1;
                swidth = 1;
                break;
            case defect::odd_length:
                length = 3;
                swidth = 3;
                break;
            case defect::oversized_length:
                length = w + 1;
                swidth = w + 1;
                break;
            case defect::unaligned_address:
                address = geo.valid_address + 1;
                break;
            case defect::address_past_aperture:
                address = geo.aperture_bytes;
                break;
            case defect::address_wrap: {
                // Aligned, and `address + length` still overflows 64 bits.
                // An unaligned value would be rejected by the width check
                // before the wrap check ever ran.
                const uint64_t mask = static_cast<uint64_t>(w) - 1u;
                address = (UINT64_MAX - (static_cast<uint64_t>(w) - 1u)) & ~mask;
                break;
            }
            case defect::byte_enable_all_disabled:
            case defect::byte_enable_one_hot:
            case defect::byte_enable_alternating:
            case defect::byte_enable_short:
                break;  // handled below, after buffers are sized
            case defect::streaming_width_zero:
                swidth = 0;
                break;
            case defect::streaming_width_partial:
                swidth = (w > 1) ? (w / 2) : 1;
                break;
            case defect::streaming_width_excess:
                swidth = w * 2;
                break;
            case defect::ignore_command:
                cmd = tlm::TLM_IGNORE_COMMAND;
                break;
            case defect::stale_dmi_allowed:
                dmi = true;
                break;
        }

        data_.assign(length ? length : 1u, 0xA5);

        switch (d) {
            case defect::byte_enable_all_disabled:
                be_.assign(length, BYTE_DISABLED);
                break;
            case defect::byte_enable_one_hot:
                be_.assign(length, BYTE_DISABLED);
                if (!be_.empty())
                    be_[0] = BYTE_ENABLED;
                break;
            case defect::byte_enable_alternating:
                be_.resize(length);
                for (unsigned i = 0; i < length; ++i)
                    be_[i] = (i % 2 == 0) ? BYTE_ENABLED
                                          : BYTE_DISABLED;
                break;
            case defect::byte_enable_short:
                // Half-width pattern: TLM-2.0 says the target repeats it across
                // the transfer. Rarely implemented, rarely rejected explicitly.
                be_.assign((length > 1) ? length / 2 : 1, BYTE_ENABLED);
                break;
            default:
                break;
        }

        gp_.set_command(cmd);
        gp_.set_address(address);
        gp_.set_data_ptr(null_ptr ? nullptr : data_.data());
        gp_.set_data_length(length);
        gp_.set_streaming_width(swidth);
        gp_.set_byte_enable_ptr(be_.empty() ? nullptr : be_.data());
        gp_.set_byte_enable_length(static_cast<unsigned>(be_.size()));
        gp_.set_dmi_allowed(dmi);
        gp_.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    }

    malformed_payload(const malformed_payload&)            = delete;
    malformed_payload& operator=(const malformed_payload&) = delete;

    tlm::tlm_generic_payload& payload()       { return gp_; }
    defect                    kind()    const { return defect_; }
    const char*               name()    const { return defect_name(defect_); }

private:
    defect                     defect_;
    tlm::tlm_generic_payload   gp_;
    std::vector<unsigned char> data_;
    std::vector<unsigned char> be_;
};

/// Convenience: drive one defect and return the outcome.
///
/// Expectations are intentionally not encoded. Policy differs per IP — one
/// target legitimately supports byte enables while its neighbour must reject
/// them — so the caller asserts the contract its RDL specifies.
template <typename Socket>
access_result probe_defect(Socket& sock,
                           defect d,
                           const target_geometry& geo,
                           tlm::tlm_command base_cmd = tlm::TLM_WRITE_COMMAND,
                           sc_core::sc_time incoming = sc_core::SC_ZERO_TIME)
{
    malformed_payload mp(d, geo, base_cmd);
    return access(sock, mp.payload(), incoming);
}

}  // namespace simtlm
