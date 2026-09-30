// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

/**
 * @file sep_reset_ctrl_test.h
 * @brief TLM initiator harness for SEP Reset Controller tests
 */

#pragma once
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <cstdint>

class sep_reset_ctrl_test : public sc_core::sc_module
{
public:
    SC_HAS_PROCESS(sep_reset_ctrl_test);

    tlm_utils::simple_initiator_socket<sep_reset_ctrl_test> initiator_socket;

    explicit sep_reset_ctrl_test(sc_core::sc_module_name n);
};
