#pragma once

// SystemC wrapper that surfaces the SEP bootcode *production* status stream as
// host-console "SEP_STATUS" lines through the shared CSML logger.
//
// The production path is the status ring buffer in SMC SRAM that the SMC reads on
// silicon (fw/sep/bootcode/src/status_ring.c). The pure decode + TSV-parse logic live
// in the SystemC-free StatusDecoder; this class adds CCI configuration, the run-time
// value->name table load, and CSML emission. It is the structured-status peer of the
// free-form-debug SIM_OUT console (sep/peripherals/sep_virt_console).
//
// Wiring: the platform injects an observation-only write-tap into the smc_global
// SEPMemory instance, filtered to the ring buffer's entries[] window, and forwards each
// 32-bit entry write here. The ring is never modified (reads/writes keep RW semantics).

#include <cstdint>
#include <string>

#include <systemc>

#include "csml_logger.h"
#include "csml_parameter.h"
#include "sep_status_decoder.h"

#ifndef CSML_DEFAULT_VERBOSITY
#define CSML_DEFAULT_VERBOSITY 2
#endif

class SepStatusReport : public sc_core::sc_module {
  public:
    CsmlLogger logger;
    csml_param<int> verbosity;          ///< CSML verbosity gate for the emitted lines
    csml_param<bool> enable;            ///< Master on/off (default true)
    csml_param<std::string> names_tsv;  ///< Path to value->name TSV (loaded at construction)

    explicit SepStatusReport(sc_core::sc_module_name name);
    ~SepStatusReport() override = default;

    /// Observe one 32-bit status word written to a ring entries[] slot.
    void on_word(uint32_t word) { decoder_.on_word(word); }

    /// Observe a raw bus-write payload (assembles a little-endian word; ignores
    /// sub-word writes). offset is accepted for symmetry with the write-tap signature.
    void on_bytes(uint64_t /*offset*/, const uint8_t* data, unsigned len) {
        decoder_.on_bytes(data, len);
    }

    /// True when output is enabled; the platform uses this to decide whether to install
    /// the write-tap at all. Non-const because csml_param::get_param_value() is non-const.
    bool enabled() { return enable.get_param_value(); }

  private:
    void load_names();  ///< Parse names_tsv into the decoder; report once on failure.

    sep_status_report::StatusDecoder decoder_;
};
