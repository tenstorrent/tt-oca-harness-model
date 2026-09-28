// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "sep_scratch_warm_basetest.h"

// The register-property table that used to live here described one SCRATCH
// entry and was never read by the suite. The testbench derives offsets from
// SCRATCH_STRIDE and asserts masks directly, so the table was dead metadata
// that could drift from the model without any test noticing.
