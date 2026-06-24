#pragma once

// SystemC wrapper that surfaces SEP bootcode `simput*` status as host-console
// "SIM_OUT" lines, printed through the shared CSML logger the same way internal
// VP status is printed. The decode logic lives in the SystemC-free
// VirtConsoleDecoder; this class adds CCI configuration and CSML emission.
//
// Wiring: the platform injects a write-tap into the sep_scratch SEPMemory instance
// and forwards writes to SEP_SCRATCH_COLD_SCRATCH_2 (offset 0x10) here. Reads/writes
// of the register are unaffected (observation only).

#include <cstdint>

#include <systemc>

#include "csml_logger.h"
#include "csml_parameter.h"
#include "virt_console_decoder.h"

#ifndef CSML_DEFAULT_VERBOSITY
#define CSML_DEFAULT_VERBOSITY 2
#endif

class SimVirtConsole : public sc_core::sc_module {
  public:
    CsmlLogger logger;
    csml_param<int> verbosity;   ///< CSML verbosity gate for the emitted lines
    csml_param<bool> enable;     ///< Master on/off (default true)

    explicit SimVirtConsole(sc_core::sc_module_name name);
    ~SimVirtConsole() override;

    /// Observe one 32-bit write to SEP_SCRATCH_COLD_SCRATCH_2.
    void on_word(uint32_t word) { decoder_.on_word(word); }

    /// Observe a raw bus-write payload (assembles a little-endian word; ignores
    /// sub-word writes). offset is accepted for symmetry with the tap signature.
    void on_bytes(uint64_t /*offset*/, const uint8_t* data, unsigned len) {
        decoder_.on_bytes(data, len);
    }

    /// True when output is enabled; the platform uses this to decide whether to
    /// install the write-tap at all. Non-const because csml_param::get_param_value()
    /// is non-const.
    bool enabled() { return enable.get_param_value(); }

  private:
    sep_virt_console::VirtConsoleDecoder decoder_;
};
