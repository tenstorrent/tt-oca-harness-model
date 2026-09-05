// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file tlm_quantum_policy.h
 * @brief Process-wide TLM-2.0 global quantum: set once, then only fill if unset.
 *
 * Accellera provides a single `tlm::tlm_global_quantum` per SystemC process.
 * A dual-ISS platform must install that value in `sc_main` (or the composing
 * platform) *before* either ISS is constructed. Subsystems and peripherals
 * must not overwrite a quantum that is already in force — they may install a
 * local default only when the singleton is still `SC_ZERO_TIME`, which is how
 * standalone SMC-only / SEP-only tests and unit testbenches keep working.
 */
#pragma once

#include <cstdint>
#include <tlm.h>

namespace simtlm {

/// Single process-wide default (1 µs). Matches SMC `quantum_insts` × 1 ns
/// and SEP `instrBatchSize` × 10 ns/cycle. Sweeped on smu-vp dual-image
/// tests (1..100000 ns): wall time is flat; 1000 ns is the LT convention.
inline constexpr uint64_t DEFAULT_GLOBAL_QUANTUM_NS = 1000;

inline bool global_quantum_is_unset()
{
    return tlm::tlm_global_quantum::instance().get() == sc_core::SC_ZERO_TIME;
}

/// Top-level / sc_main: install the process-wide quantum.
inline void set_global_quantum(const sc_core::sc_time& q)
{
    tlm::tlm_global_quantum::instance().set(q);
}

inline void set_global_quantum_ns(uint64_t ns)
{
    set_global_quantum(sc_core::sc_time(static_cast<double>(ns), sc_core::SC_NS));
}

/// Subsystem / ISS / peripheral: install only if nobody has set a quantum yet.
/// Returns true if this call wrote the singleton.
inline bool install_global_quantum_if_unset(const sc_core::sc_time& q)
{
    if (!global_quantum_is_unset() || q == sc_core::SC_ZERO_TIME)
        return false;
    set_global_quantum(q);
    return true;
}

inline bool install_global_quantum_ns_if_unset(uint64_t ns)
{
    return install_global_quantum_if_unset(
        sc_core::sc_time(static_cast<double>(ns), sc_core::SC_NS));
}

}  // namespace simtlm
