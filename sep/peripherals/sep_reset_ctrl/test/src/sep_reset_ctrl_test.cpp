// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file sep_reset_ctrl_test.cpp
 * @brief Initiator socket harness for SEP Reset Controller
 */

#include "sep_reset_ctrl_test.h"

sep_reset_ctrl_test::sep_reset_ctrl_test(sc_core::sc_module_name n)
    : sc_module(n), initiator_socket("initiator_socket")
{
}
