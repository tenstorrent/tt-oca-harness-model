// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "sep_scratch_cold_basetest.h"

sep_scratch_cold_basetest::Register_Property_t reg_map[1] = {
{sep_scratch_cold_basetest::SCRATCH_OFFSET, sep_scratch_cold_basetest::SCRATCH_READ, sep_scratch_cold_basetest::SCRATCH_WRITE, sep_scratch_cold_basetest::SCRATCH_RESET, "SCRATCH"}};