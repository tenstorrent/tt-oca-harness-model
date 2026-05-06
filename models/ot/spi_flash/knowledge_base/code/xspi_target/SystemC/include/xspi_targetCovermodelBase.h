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
 * CHECKSUM:73a2e21593290c498aefb7589e45291bc8d11542
 ***************************************************************************/
 
#pragma once

#include "scml2_coverage.h"
#include "xspi_target.h"
#include "../../SystemC/include/sfdp.h"
#include "../../SystemC/include/xspi_target_lib.h"
#include "../../SystemC/include/extension.h"

#ifdef _WIN32
#pragma optimize("g",off)
#else
#pragma GCC push_options
#pragma GCC optimize("O0")
#endif


class xspi_targetCovermodelBase
#ifdef SNPS_SLS_VP_COVERAGE
  : public scml2::cov::covergroup
#endif
{
public:
 xspi_targetCovermodelBase(const std::string& test_name, xspi_target& t)
 #ifdef SNPS_SLS_VP_COVERAGE
  : scml2::cov::covergroup("xspi_target", test_name, &t)
  , MEM_SIZE(t.MEM_SIZE, "MEM_SIZE")
  , reset_in(t.reset_in, "reset_in")
  , mem(t.mem, "mem")
 #endif
 {
 #ifdef SNPS_SLS_VP_COVERAGE

  this->MEM_SIZE.disable();
 #endif
 }
 #ifdef SNPS_SLS_VP_COVERAGE
protected:
 scml2::cov::scml_property<int> MEM_SIZE;
 scml2::cov::sc_in<bool> reset_in;
 scml2::cov::memory<unsigned int> mem;
 #endif
};
#ifdef _WIN32
#pragma optimize("",on)
#else
#pragma GCC pop_options
#endif



