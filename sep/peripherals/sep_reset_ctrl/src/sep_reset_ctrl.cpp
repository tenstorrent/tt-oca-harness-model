// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file sep_reset_ctrl.cpp
 * @brief SEP Software Reset Controller implementation
 * 
 * Simple implementation - just one register callback following AES pattern.
 */

#include "sep_reset_ctrl.h"
#include "reg_access.h"

// Constructor - follow AES pattern exactly
sep_reset_ctrl_ip::sep_reset_ctrl_ip(sc_module_name n)
    : sep_reset_ctrl_base(n, "sep_reset_ctrl", 0x8)
    , verbosity("verbosity", REG_DEFAULT_VERBOSITY)
{
    logger.setMaxVerbosity(verbosity.get_param_value());
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    // Register SC_METHOD to update reset outputs. Deliberately initialized rather
    // than dont_initialize()'d: otherwise the outputs sit at sc_signal's default
    // false — every crypto IP held in reset — until something produces the first
    // edge on global_rst_ni or SW_RESET_N. That happens to work when the parent
    // writes the reset signal during elaboration, because the false-to-true
    // update counts as a change, but it silently does not when the parent
    // constructs the signal already true (sc_signal's initial-value ctor): no
    // edge, no run, and the whole accelerator set stays reset for the entire
    // simulation. Running once at t=0 makes the initial state ours to define.
    SC_METHOD(update_rst_outputs);
    sensitive << global_rst_ni << sw_reset_changed_;

    // Register SC_METHOD to clear SW_RESET_N back to its default on reset
    SC_METHOD(reset_handler);
    sensitive << global_rst_ni;
    dont_initialize();

    // Prefer the byte-enable callback so full-width writes with a BE array
    // merge against the live shadow. The plain write_callback path only merges
    // for short (len < 8) transfers; a full-length BE write would otherwise
    // replace the whole word and ignore disabled lanes.
    std::function<bool(uint64_t, uint8_t)> write_cb =
        [this](uint64_t value, uint8_t byte_enable) {
            const uint64_t merged = regmodel::apply_byte_enable(
                static_cast<uint64_t>(SW_RESET_N), value, byte_enable);
            return this->handle_sw_reset_n_write(merged, SW_RESET_N.write_bit_mask);
        };
    memory.register_write_callback_with_be(write_cb, SW_RESET_N.offset);

    std::function<bool(unsigned long long&)> read_cb = [this](unsigned long long& value) {
        value = static_cast<uint64_t>(SW_RESET_N) & SW_RESET_N.read_bit_mask;
        return true;
    };
    memory.register_read_callback(read_cb, SW_RESET_N.offset);
}

// Reset function 
void sep_reset_ctrl_ip::reset()
{
    // reset_all_registers() restores SW_RESET_N to its declared reset value
    // (0x7e). That register word is the model's only copy of the software
    // reset state, so there is nothing else to restore here -- re-stating the
    // default would just be a second place to keep in sync.
    reset_all_registers();

    // Notify that reset values are applied
    sw_reset_changed_.notify(sc_core::SC_ZERO_TIME);
}

// SC_METHOD: fires on any global_rst_ni edge; clears SW_RESET_N when low
void sep_reset_ctrl_ip::reset_handler()
{
    if (!global_rst_ni.read())
        reset();
}

// SC_METHOD: Update reset output signals (knowledge-base logic)
void sep_reset_ctrl_ip::update_rst_outputs()
{
    const bool global_rst = global_rst_ni.read();

    // Each output is the global reset ANDed with its per-IP software reset
    // bit. Read the bits through SW_RESET_N's named bitfields so the wiring
    // is checked against the register description rather than against a
    // separate table of hand-written bit positions.
    auto sw = [](const regmodel::Bitfield<64>& f) {
        return static_cast<uint64_t>(f) != 0u;
    };

    km_rst_ni.write(global_rst   && sw(SW_RESET_N.km_sw_rst_n));
    otbn_rst_n.write(global_rst  && sw(SW_RESET_N.otbn_sw_rst_n));
    aes_rst_ni.write(global_rst  && sw(SW_RESET_N.aes_sw_rst_n));
    hmac_rst_ni.write(global_rst && sw(SW_RESET_N.hmac_sw_rst_n));
    kmac_rst_ni.write(global_rst && sw(SW_RESET_N.kmac_sw_rst_n));
    trng_rst_ni.write(global_rst && sw(SW_RESET_N.trng_sw_rst_n));
    abr_rst_ni.write(global_rst  && sw(SW_RESET_N.abr_sw_rst_n));
}

// Write callback - AES pattern
bool sep_reset_ctrl_ip::handle_sw_reset_n_write(uint64_t value, uint64_t mask)
{
    // This callback replaces Reg::handle_write, so the register-file word is
    // not updated unless we do it here -- and since SW_RESET_N is the model's
    // only copy of the state, leaving it at the power-on reset value would
    // make later partial writes combine against a stale 0x7e instead of the
    // live CSR, and would make reads return the wrong value.
    SW_RESET_N = value & mask;
    sw_reset_changed_.notify(sc_core::SC_ZERO_TIME);
    // Yield to the SC kernel so update_rst_outputs fires immediately and the
    // downstream peripheral SC_METHODs see the reset signal go low before this
    // b_transport returns. Without this, back-to-back assert+de-assert writes
    // from the ISS SC_THREAD both land in the same delta cycle: the sc_event
    // notification is merged and update_rst_outputs only ever fires with the
    // final (de-asserted) value, so peripheral registers are never cleared.
    //
    // Only a thread may wait. regmodel's write_registers() takes an is_debug flag but
    // ignores it, so a transport_dbg write — GDB poking this register, say —
    // reaches here with no process context at all, and an unguarded wait() would
    // be a fatal SystemC error. The debug path gets the event without the yield,
    // which is right: it is not modelling a timed bus access to begin with.
    const auto kind = sc_core::sc_get_current_process_handle().proc_kind();
    if (kind == sc_core::SC_THREAD_PROC_ || kind == sc_core::SC_CTHREAD_PROC_) {
        sc_core::wait(sc_core::SC_ZERO_TIME);
    }
    return true;
}