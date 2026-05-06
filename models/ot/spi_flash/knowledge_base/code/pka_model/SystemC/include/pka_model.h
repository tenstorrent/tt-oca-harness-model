/***************************************************************************
 * Copyright 1996-2025 Synopsys, Inc.
 *
 * This Synopsys software and all associated documentation are proprietary
 * to Synopsys, Inc. and may only be used pursuant to the terms and
 * conditions of a written license agreement with Synopsys, Inc.
 * All other use, reproduction, modification, or distribution of the
 * Synopsys software or the associated documentation is strictly prohibited.
 ***************************************************************************/
 

/***************************************************************************
 * Generated snippet, used for detecting user edits.
 * CHECKSUM:d97abd21e10c52bedfb023b6b078f77395d938e9
 ***************************************************************************/
 
#pragma once

#include "pka_modelBase.h"

namespace mylibrary {

/**
 * \copydoc pka_modelBase
 */
class pka_model : public pka_modelBase {
  public:
#if (defined SYSTEMC_VERSION && SYSTEMC_VERSION <= 20221128 /*2.3.4*/)
    SC_HAS_PROCESS(pka_model);
#endif

    pka_model(sc_core::sc_module_name name);
  #ifdef ACCELLERA_SYSTEMC
  virtual void process_params(const std::map<std::string, std::string> & params);
  #endif
  private:
    friend class pka_modelCovermodel;

    virtual void handle_write_bank_A(tlm::tlm_generic_payload& payload, sc_core::sc_time& time);
    virtual void handle_write_bank_B(tlm::tlm_generic_payload& payload, sc_core::sc_time& time);
    virtual void handle_write_bank_C(tlm::tlm_generic_payload& payload, sc_core::sc_time& time);
    virtual void handle_write_bank_D(tlm::tlm_generic_payload& payload, sc_core::sc_time& time);
    virtual void handle_write_pka_fw(tlm::tlm_generic_payload& payload, sc_core::sc_time& time);
    virtual bool handle_write_FLAGS(const unsigned int &value, const unsigned int &byteEnables, sc_core::sc_time &time) override;

  private:
        
};

}  // end of namespace mylibrary
