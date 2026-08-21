#include "sep_filter_ctrl.h"

#include "sep_axi_extension.h"

namespace {

// CSML 64-bit words receive RV32 stores as two 32-bit TLM beats. The legacy
// write callback is given only the bytes in this beat (other bytes zero), so
// a high-half store of 0 would wipe START/END/FILTER_CONFIG. Merge with the
// current word using the beat's byte-enable before applying register logic.
uint64_t byte_enable_to_mask(uint8_t be)
{
    uint64_t mask = 0;
    for (unsigned b = 0; b < 8; ++b) {
        if ((be & static_cast<uint8_t>(1u << b)) != 0)
            mask |= 0xFFull << (b * 8);
    }
    return mask;
}

uint64_t merge_be(uint64_t current, uint64_t value, uint8_t be)
{
    const uint64_t mask = byte_enable_to_mask(be);
    return (current & ~mask) | (value & mask);
}

}  // namespace

// =============================================================================
// InstanceType constructor — delegates to the parametric constructor
// =============================================================================
sep_filter_ctrl_ip::sep_filter_ctrl_ip(sc_module_name n, InstanceType type)
    : sep_filter_ctrl_ip(n,
        type == InstanceType::OUTBOUND ? OUTBOUND_NUM_INSTANCES : INBOUND_NUM_INSTANCES)
{}

// =============================================================================
// Parametric constructor
// =============================================================================
sep_filter_ctrl_ip::sep_filter_ctrl_ip(sc_module_name n, uint32_t num_instances)
    : sep_filter_ctrl_base(n, "sep_filter_ctrl", num_instances)
    , data_socket("data_socket")
    , filtered_socket("filtered_socket")
    , rst_ni("rst_ni")
    , filter_skip_i("filter_skip_i")
    , verbosity("verbosity", CSML_DEFAULT_VERBOSITY)
    , filter_skip_tie_low_("filter_skip_tie_low", false)
{
    logger.setMaxVerbosity(verbosity.get_param_value());
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    SC_METHOD(reset_handler);
    sensitive << rst_ni;
    dont_initialize();

    if (num_instances == 0 || num_instances > MAX_INSTANCES) {
        SC_REPORT_FATAL("sep_filter_ctrl",
            "num_instances must be in [1, MAX_INSTANCES]");
    }

    data_socket.register_b_transport(this,   &sep_filter_ctrl_ip::data_b_transport);
    data_socket.register_transport_dbg(this, &sep_filter_ctrl_ip::data_transport_dbg);

    reset();

    // Register RDL-compliant callbacks for all instances. Write callbacks are
    // byte-enable aware so a 32-bit RV32 store updates only that half of the
    // 64-bit CSR (smu-aou-ext-test programs these with wr64 = two sw).
    for (uint32_t i = 0; i < num_instances_; ++i) {
        uint32_t offset = FILTER_CONFIG[i].offset;

        memory.register_write_callback_with_be(
            [this, i](DT value, uint8_t be) -> bool {
                const DT merged = merge_be(static_cast<uint64_t>(FILTER_CONFIG[i]),
                                           static_cast<uint64_t>(value), be);
                return this->handle_filter_config_write(i, merged);
            },
            offset);

        memory.register_read_callback(
            [this, i](DT& value) -> bool {
                return this->handle_filter_config_read(i, value);
            },
            offset);

        // START_ADDR / END_ADDR are frozen once this entry's locked bit is set
        // ("Write once register to lock filter configurations" — RDL).
        memory.register_write_callback_with_be(
            [this, i](DT value, uint8_t be) -> bool {
                if (this->get_filter_entry(i).locked) return true;
                const DT merged = merge_be(static_cast<uint64_t>(START_ADDR[i]),
                                           static_cast<uint64_t>(value), be);
                bool ok = this->START_ADDR[i].handle_write(merged, this->START_ADDR[i].write_bit_mask);
                this->auto_correct_start_end(i);
                return ok;
            },
            START_ADDR[i].offset);

        memory.register_write_callback_with_be(
            [this, i](DT value, uint8_t be) -> bool {
                if (this->get_filter_entry(i).locked) return true;
                const DT merged = merge_be(static_cast<uint64_t>(END_ADDR[i]),
                                           static_cast<uint64_t>(value), be);
                bool ok = this->END_ADDR[i].handle_write(merged, this->END_ADDR[i].write_bit_mask);
                this->auto_correct_start_end(i);
                return ok;
            },
            END_ADDR[i].offset);
    }
}

// =============================================================================
// before_end_of_elaboration — bind the tie-off if the parent left filter_skip_i
// open. Required for anything that calls sc_start(), which completes port
// binding and aborts with E109 on an unbound sc_in.
// =============================================================================
void sep_filter_ctrl_ip::before_end_of_elaboration()
{
    if (!filter_skip_i.get_interface())
        filter_skip_i(filter_skip_tie_low_);
}

// =============================================================================
// reset
// =============================================================================
void sep_filter_ctrl_ip::reset()
{
    reset_all_registers();
}

// =============================================================================
// get_filter_entry — decode one register set into a FilterEntry struct
// =============================================================================
sep_filter_ctrl_ip::FilterEntry sep_filter_ctrl_ip::get_filter_entry(uint32_t idx) const
{
    if (idx >= num_instances_) {
        SC_REPORT_ERROR("sep_filter_ctrl", "get_filter_entry: idx out of range");
        return FilterEntry{};
    }

    uint64_t config_val = static_cast<uint64_t>(FILTER_CONFIG[idx]);

    FilterEntry entry;
    entry.read_allowed  = static_cast<bool>((config_val >>  0) & 0x1);
    entry.write_allowed = static_cast<bool>((config_val >>  1) & 0x1);
    entry.entry_enabled = static_cast<bool>((config_val >>  4) & 0x1);
    entry.allow_ns      = static_cast<bool>((config_val >>  8) & 0x1);
    entry.src_id        = static_cast<uint8_t>((config_val >> 16) & 0xF);
    entry.group_id      = static_cast<uint8_t>((config_val >> 20) & 0xF);
    entry.allow_burst   = static_cast<bool>((config_val >> 24) & 0x1);
    entry.locked        = static_cast<bool>((config_val >> 63) & 0x1);

    entry.start_addr = static_cast<uint64_t>(START_ADDR[idx]) & ADDR_FIELD_MASK;
    entry.end_addr   = static_cast<uint64_t>(END_ADDR[idx])   & ADDR_FIELD_MASK;

    return entry;
}

// =============================================================================
// check_and_forward — core filter enforcement
//
// Mirrors traffic_filter.sv's split between the "hit" term and the permission
// check made at the hit entry:
//
//   filter_hit = entry_enabled & in_range & pass_src_id & pass_ns
//                & pass_burst & pass_group_id
//
// Everything in that AND is a hit condition, so an entry failing any of it is
// simply not the match — the scan continues to the next entry. read_allowed /
// write_allowed are deliberately NOT part of it (they feed tx_rule_pass_o,
// checked by axi_filter_wrap.sv at the winning index), so a matching entry that
// forbids the direction denies outright rather than deferring to a later entry.
//
// src_id and allow_ns come off the sep_axi_extension attached by the initiator;
// group_id is not checked because SEP wires EnGroupIdFilter=0 on both filter
// instances (sep_system_peripherals.sv:345,392), leaving pass_group_id at 1.
//
// BlockByDefault: RTL's axi_filter_wrap sets BlockByDefault=1'b1 for both the
// SEP outbound_filter and inbound_filter instances (sep_system_peripherals.sv).
// The RTL's lzc "empty" hit-vector flag is identical whether zero entries are
// enabled or N entries are enabled but none match — there is no separate
// "unconfigured = passthrough" state. No match (for any reason) always denies.
// =============================================================================
int sep_filter_ctrl_ip::match_entry(const tlm::tlm_generic_payload& trans) const
{
    const uint64_t addr = trans.get_address();

    // An unextended transaction is treated as a trusted local master, using
    // sep_axi_extension's own field defaults (SEP_SOURCE_ID, secure). That
    // keeps models and test benches predating the extension behaving as before,
    // and keeps the defaults defined in one place.
    static const sep::sep_axi_extension DEFAULT_ATTRS;
    const sep::sep_axi_extension* ext =
        trans.get_extension<sep::sep_axi_extension>();
    const sep::sep_axi_extension& attrs = ext ? *ext : DEFAULT_ATTRS;

    for (uint32_t i = 0; i < num_instances_; ++i) {
        FilterEntry entry = get_filter_entry(i);
        if (!entry.entry_enabled) continue;

        // traffic_filter.sv: range check is masked to page (allow_burst) or
        // 8-byte word (!allow_burst) granularity, not an exact byte compare.
        const unsigned shift    = entry.allow_burst ? 12 : DATA_BUS_WIDTH;
        const uint64_t addr_g   = addr             >> shift;
        const uint64_t start_g  = entry.start_addr >> shift;
        const uint64_t end_g    = entry.end_addr   >> shift;

        if (addr_g < start_g || addr_g > end_g) continue;

        // pass_src_id — a zero CSR field is the wildcard, so an entry that
        // names no source admits every master. Note this keys off the CSR
        // field being zero, not the transaction's; pass_group_id has the
        // opposite polarity in the RTL, which is why they are not shared.
        if (entry.src_id != 0 && attrs.source_id != entry.src_id) continue;

        // pass_ns — an exact match, not a permission bit: allow_ns names the
        // one security level the entry admits.
        if (attrs.is_ns != entry.allow_ns) continue;

        // pass_burst
        if (!entry.allow_burst && trans.get_data_length() > SINGLE_BEAT_BYTES) continue;

        return static_cast<int>(i);
    }

    return -1;
}

bool sep_filter_ctrl_ip::check_and_forward(tlm::tlm_generic_payload& trans,
                                            sc_core::sc_time& delay)
{
    // filter_skip_i forces isolate_write/isolate_read low, so nothing is
    // checked at all — not the match, not the permissions.
    if (filter_skip()) {
        filtered_socket->b_transport(trans, delay);
        return true;
    }

    const int idx = match_entry(trans);
    if (idx < 0) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return false;
    }

    const FilterEntry      entry = get_filter_entry(static_cast<uint32_t>(idx));
    const tlm::tlm_command cmd   = trans.get_command();

    if (cmd == tlm::TLM_READ_COMMAND  && !entry.read_allowed) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return false;
    }
    if (cmd == tlm::TLM_WRITE_COMMAND && !entry.write_allowed) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return false;
    }

    filtered_socket->b_transport(trans, delay);
    return true;
}

// =============================================================================
// data_b_transport
// =============================================================================
void sep_filter_ctrl_ip::data_b_transport(tlm::tlm_generic_payload& trans,
                                           sc_core::sc_time& delay)
{
    check_and_forward(trans, delay);
}

// =============================================================================
// data_transport_dbg — same filter logic, returns 0 on denial
// =============================================================================
unsigned int sep_filter_ctrl_ip::data_transport_dbg(tlm::tlm_generic_payload& trans)
{
    if (filter_skip())
        return filtered_socket->transport_dbg(trans);

    const int idx = match_entry(trans);
    // BlockByDefault=1 — no match (unconfigured or non-matching) always denies.
    if (idx < 0) return 0;

    const FilterEntry      entry = get_filter_entry(static_cast<uint32_t>(idx));
    const tlm::tlm_command cmd   = trans.get_command();

    if (cmd == tlm::TLM_READ_COMMAND  && !entry.read_allowed)  return 0;
    if (cmd == tlm::TLM_WRITE_COMMAND && !entry.write_allowed) return 0;

    return filtered_socket->transport_dbg(trans);
}

// =============================================================================
// RDL callbacks — enforce data_bus_width=3 (hw=w) and locked WOSET
// =============================================================================
bool sep_filter_ctrl_ip::handle_filter_config_write(uint32_t idx, DT value)
{
    if (idx >= num_instances_) {
        SC_REPORT_ERROR("sep_filter_ctrl", "handle_filter_config_write: idx out of range");
        return false;
    }

    uint64_t current_val = static_cast<uint64_t>(FILTER_CONFIG[idx]);

    // locked[63] freezes the entire filter configuration (FILTER_CONFIG,
    // START_ADDR, END_ADDR) — once set it can never be cleared or bypassed.
    bool current_locked = (current_val >> 63) & 0x1;
    if (current_locked)
        return true;

    uint64_t new_val = static_cast<uint64_t>(value);

    // data_bus_width[14:12] is hw=w — always 3
    new_val = (new_val & ~0x7000ULL) | 0x3000ULL;

    // locked[63] is WOSET — write-one-to-set, sticky
    if ((new_val >> 63) & 0x1)
        new_val |= (1ULL << 63);

    FILTER_CONFIG[idx] = static_cast<DT>(new_val);

    // allow_burst[24] may have just changed, which changes the granularity
    // (page vs word) used by the start/end auto-correction below.
    auto_correct_start_end(idx);
    return true;
}

bool sep_filter_ctrl_ip::handle_filter_config_read(uint32_t idx, DT& value)
{
    if (idx >= num_instances_) {
        SC_REPORT_ERROR("sep_filter_ctrl", "handle_filter_config_read: idx out of range");
        return false;
    }

    uint64_t current_val = static_cast<uint64_t>(FILTER_CONFIG[idx]);
    current_val = (current_val & ~0x7000ULL) | 0x3000ULL;
    value = static_cast<DT>(current_val);
    return true;
}

// =============================================================================
// auto_correct_start_end — mirrors axi_filter_wrap.sv's combinational
// start/end auto-correction. If start_addr and end_addr fall within the same
// page (allow_burst) or same 8-byte word (!allow_burst), hardware snaps the
// STORED register values to the full enclosing page/word boundary.
// =============================================================================
void sep_filter_ctrl_ip::auto_correct_start_end(uint32_t idx)
{
    if (idx >= num_instances_) {
        SC_REPORT_ERROR("sep_filter_ctrl", "auto_correct_start_end: idx out of range");
        return;
    }

    const uint64_t config_val  = static_cast<uint64_t>(FILTER_CONFIG[idx]);
    const bool     allow_burst = (config_val >> 24) & 0x1;
    const unsigned shift       = allow_burst ? 12 : DATA_BUS_WIDTH;

    const uint64_t start_addr = static_cast<uint64_t>(START_ADDR[idx]) & ADDR_FIELD_MASK;
    const uint64_t end_addr   = static_cast<uint64_t>(END_ADDR[idx])   & ADDR_FIELD_MASK;

    if ((start_addr >> shift) == (end_addr >> shift)) {
        const uint64_t page_base = (start_addr >> shift) << shift;
        const uint64_t page_top  = page_base | ((1ULL << shift) - 1ULL);
        START_ADDR[idx] = static_cast<DT>(page_base);
        END_ADDR[idx]   = static_cast<DT>(page_top);
    }
}

// =============================================================================
// reset_handler — fires on any rst_ni edge; clears all entries when low
// =============================================================================
void sep_filter_ctrl_ip::reset_handler()
{
    if (!rst_ni.read())
        reset();
}
