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
 * CHECKSUM:5daa608e6d37c976ee8061a27cdca86165198fad
 ***************************************************************************/
 
#pragma once

#include "scml2_coverage.h"

#include "xspi_targetCovermodelBase.h"



class xspi_targetCovermodel
  : public xspi_targetCovermodelBase
{
public:
 xspi_targetCovermodel(const std::string& test_name, xspi_target& t)
  : xspi_targetCovermodelBase(test_name, t)
 {
#ifdef SNPS_SLS_VP_COVERAGE
#endif
 }

 virtual ~xspi_targetCovermodel() {
  this->write_log();
 }
};

