// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#ifndef COMMON_SEP_DEFINES
#define COMMON_SEP_DEFINES

// Simulation control mailbox (also prints ASCII chars); same address as
// STDOUT in tb.h (tt-oca-harness #2589 unified the two).
#define STDOUT 0x80000000

#endif
