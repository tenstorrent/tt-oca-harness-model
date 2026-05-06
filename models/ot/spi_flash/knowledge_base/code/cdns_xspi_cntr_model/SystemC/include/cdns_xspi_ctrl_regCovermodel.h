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
 * CHECKSUM:918e7044e2c0dc451e1273e036d77eda52e33579
 ***************************************************************************/
 
#pragma once

#include "scml2_coverage.h"

#include "cdns_xspi_ctrl_regCovermodelBase.h"

namespace mylibrary {


class cdns_xspi_ctrl_regCovermodel
  : public cdns_xspi_ctrl_regCovermodelBase
{
public:
 cdns_xspi_ctrl_regCovermodel(const std::string& test_name, cdns_xspi_ctrl_reg& t)
  : cdns_xspi_ctrl_regCovermodelBase(test_name, t)
 {
#ifdef SNPS_SLS_VP_COVERAGE
#endif
 }

 virtual ~cdns_xspi_ctrl_regCovermodel() {
  this->write_log();
 }
};

}  // end of namespace mylibrary
