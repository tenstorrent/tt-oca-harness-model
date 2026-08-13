/**
 * @file sep_reset_ctrl.cpp
 * @brief SEP Software Reset Controller implementation
 * 
 * Simple implementation - just one register callback following AES pattern.
 */

#include "sep_reset_ctrl.h"

// Constructor - follow AES pattern exactly
sep_reset_ctrl_ip::sep_reset_ctrl_ip(sc_module_name n)
    : sep_reset_ctrl_base(n, "sep_reset_ctrl", 0x8)
    , verbosity("verbosity", CSML_DEFAULT_VERBOSITY)
    , sw_reset_current_value_(0x1E)
{
    logger.setMaxVerbosity(verbosity.get_param_value());
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    // Register SC_METHOD to update reset outputs
    SC_METHOD(update_rst_outputs);
    sensitive << global_rst_ni << sw_reset_changed_;
    dont_initialize();

    // Register SC_METHOD to clear SW_RESET_N back to its default on reset
    SC_METHOD(reset_handler);
    sensitive << global_rst_ni;
    dont_initialize();

    std::function<bool(uint64_t)> write_cb = [this](uint64_t value) {
        return this->handle_sw_reset_n_write(value, SW_RESET_N.write_bit_mask);
    };
    memory.register_write_callback(write_cb, SW_RESET_N.offset);

    std::function<bool(unsigned long long&)> read_cb = [this](unsigned long long& value) {
        value = sw_reset_current_value_ & SW_RESET_N.read_bit_mask;
        return true;
    };
    memory.register_read_callback(read_cb, SW_RESET_N.offset);
}

// Reset function 
void sep_reset_ctrl_ip::reset()
{
    // Reset base class registers first
    reset_all_registers();
    
    // Reset current value to reset state
    sw_reset_current_value_ = 0x1E;
    
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
    bool global_rst = global_rst_ni.read();
    
    // Use the current SW reset value for output generation
    km_rst_ni.write(global_rst   && bool(sw_reset_current_value_ & 0x01u));
    otbn_rst_n.write(global_rst  && bool(sw_reset_current_value_ & 0x02u));
    aes_rst_ni.write(global_rst  && bool(sw_reset_current_value_ & 0x04u));
    hmac_rst_ni.write(global_rst && bool(sw_reset_current_value_ & 0x08u));
    kmac_rst_ni.write(global_rst && bool(sw_reset_current_value_ & 0x10u));
}

// Write callback - AES pattern
bool sep_reset_ctrl_ip::handle_sw_reset_n_write(uint64_t value, uint64_t mask)
{
    sw_reset_current_value_ = value & mask;
    sw_reset_changed_.notify(sc_core::SC_ZERO_TIME);
    // Yield to the SC kernel so update_rst_outputs fires immediately and the
    // downstream peripheral SC_METHODs see the reset signal go low before this
    // b_transport returns. Without this, back-to-back assert+de-assert writes
    // from the ISS SC_THREAD both land in the same delta cycle: the sc_event
    // notification is merged and update_rst_outputs only ever fires with the
    // final (de-asserted) value, so peripheral registers are never cleared.
    sc_core::wait(sc_core::SC_ZERO_TIME);
    return true;
}