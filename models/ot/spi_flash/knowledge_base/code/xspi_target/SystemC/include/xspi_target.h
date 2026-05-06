/***************************************************************************
 * Copyright 1996-2024 Synopsys, Inc.
 *
 * This Synopsys software and all associated documentation are proprietary
 * to Synopsys, Inc. and may only be used pursuant to the terms and
 * conditions of a written license agreement with Synopsys, Inc.
 * All other use, reproduction, modification, or distribution of the
 * Synopsys software or the associated documentation is strictly prohibited.
 ***************************************************************************/
 

/***************************************************************************
 * Generated snippet, used for detecting user edits.
 * CHECKSUM:f9e91af257de732263fcc92a45e830945be18ef8
 ***************************************************************************/
 
#pragma once

#include "xspi_targetBase.h"
#include "sfdp.h"
#include "xspi_target_lib.h"
#include  "extension.h"
/**
 * \copydoc xspi_targetBase
 */
class xspi_target : public xspi_targetBase {
  public:
    SC_HAS_PROCESS(xspi_target);

    xspi_target(sc_core::sc_module_name name);

    virtual void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) override;
    xspi_target_model xspi_device_model;

    
    
  private:
        
};

