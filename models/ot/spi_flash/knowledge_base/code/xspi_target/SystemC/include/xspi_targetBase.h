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
 * CHECKSUM:4e14b318eac23c87987ef6031e8b65675f9e90e4
 ***************************************************************************/
 

 
 // Warning: This is an auto-generated file. All changes to this file will be overwritten.


#pragma once

#include <scml2.h>
#include <systemc>
#include <scmlinc/scml_property.h>
#include "scml2_protocol_engines/reset/include/reset_engine.h"
#include <tlm.h>




class xspi_targetCovermodelBase;


class xspi_targetCovermodel;


class xspi_target;

/**
 * This empty model example contains no predefined interfaces or registers.
It allows full flexibility in defining arbitrary components as it generates a minimal SystemC skeleton.
 * 
 */
#ifdef _WIN32
#pragma optimize("g",off)
#else
#pragma GCC push_options
#pragma GCC optimize("O0")
#endif


class xspi_targetBase : public sc_core::sc_module {
    typedef xspi_targetBase ModelBaseType;
  public:
#if (defined SYSTEMC_VERSION && SYSTEMC_VERSION <= 20221128 /*2.3.4*/)
    SC_HAS_PROCESS(xspi_targetBase);
#endif

    xspi_targetBase(sc_core::sc_module_name name) 
        : sc_core::sc_module(name)
        , MEM_SIZE("MEM_SIZE", 0x2000000)
        , reset_in("reset_in")
        , xspi_bus("xspi_bus")
        , reset_in_protocol_engine("reset_in_protocol_engine", reset_in)
        , mem("mem", (MEM_SIZE) / scml2::sizeOf<unsigned int >())
    {
      reset_in_protocol_engine.active_level = false;
      reset_in_protocol_engine.finalize_construction();
      xspi_bus.register_b_transport(this, &xspi_targetBase::b_transport);
    }
    
// LCOV_EXCL_START
    virtual ~xspi_targetBase() {
      
    }
// LCOV_EXCL_STOP    

    virtual void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) = 0;
  protected:
    /**
     * This method resets all interfaces, registers, and bitfields to their reset value.
     * It is typically called from end_of_elaboration(), and connected to the reset port.
     */
    
    virtual void reset_model() {
    }
    
    virtual void end_of_elaboration() {
      sc_core::sc_module::end_of_elaboration();
      reset_model();
    }
    
    ModelBaseType& model() {
      return *this;
    }
  
    

  protected:
    scml_property<int> MEM_SIZE;

  public:
    sc_core::sc_in<bool> reset_in;
    tlm_utils::simple_target_socket<SC_CURRENT_USER_MODULE, 64> xspi_bus;

  protected:
    friend class xspi_targetCovermodelBase;
    friend class xspi_targetCovermodel;
    friend class xspi_target;

    
    scml2::reset_slave_engine reset_in_protocol_engine;
    scml2::memory< unsigned int > mem;
private:
};
#ifdef _WIN32
#pragma optimize("",on)
#else
#pragma GCC pop_options
#endif


