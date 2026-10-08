// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
//
// GPIO interrupt reduce for smc_peripherals.sv. Every wrap, bonded or not,
// contributes. The lower half (wraps [NumGpioWraps/2-1:0]) drives peripheral
// interrupt bit 28 and the remainder drives bit 29.

#pragma once

#include <systemc>

#include <cstddef>
#include <cstdint>

namespace smc {

/// `smc_pkg::NumGpioWraps`. One total count; there is no bonded/unbonded split.
inline constexpr unsigned kNumGpioWraps = 65;

/// OR-reduce `wrap_irq[0 .. kNumGpioWraps)`. `lower` is peripheral bit 28,
/// `upper` is bit 29.
inline void gpio_irq_halves(const bool* wrap_irq, bool& lower, bool& upper)
{
    constexpr unsigned half = kNumGpioWraps / 2;  // 32
    lower = false;
    upper = false;
    for (unsigned i = 0; i < half; ++i)
        lower = lower || wrap_irq[i];
    for (unsigned i = half; i < kNumGpioWraps; ++i)
        upper = upper || wrap_irq[i];
}

/// Clockless reduce of the 65 wrap interrupts onto peripheral bits 28 and 29.
class gpio_irq_reduce : public sc_core::sc_module {
public:
    sc_core::sc_vector<sc_core::sc_in<bool>> wrap_irq;
    sc_core::sc_out<bool> lower_o{"lower_o"};
    sc_core::sc_out<bool> upper_o{"upper_o"};

    SC_HAS_PROCESS(gpio_irq_reduce);

    explicit gpio_irq_reduce(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , wrap_irq("wrap_irq", kNumGpioWraps)
    {
        SC_METHOD(recompute);
        for (unsigned i = 0; i < kNumGpioWraps; ++i)
            sensitive << wrap_irq[i];
    }

private:
    void recompute()
    {
        bool wraps[kNumGpioWraps] = {};
        for (unsigned i = 0; i < kNumGpioWraps; ++i)
            wraps[i] = wrap_irq[i].read();
        bool lower = false;
        bool upper = false;
        gpio_irq_halves(wraps, lower, upper);
        lower_o.write(lower);
        upper_o.write(upper);
    }
};

}  // namespace smc
