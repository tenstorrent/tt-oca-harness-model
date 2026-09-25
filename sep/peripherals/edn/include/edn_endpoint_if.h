// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once

#include <systemc.h>
#include <cstdint>

/**
 * LT endpoint view of the EDN native request/acknowledge interface.
 *
 * Consumers pull one 32-bit conditioned word at a time.  The concrete EDN
 * model retains ownership when no word is available, which is the functional
 * equivalent of keeping edn_ack low.  AXI/EDN cycle timing and arbitration are
 * deliberately outside this interface.
 */
class edn_endpoint_if : public sc_core::sc_interface
{
public:
    virtual bool try_pop_entropy_word(unsigned endpoint_id,
                                      uint32_t& word,
                                      bool& fips) = 0;
    virtual const sc_core::sc_event& entropy_available_event() const = 0;
};
