// SPDX-License-Identifier: Apache-2.0
/**
 * @file reg_map.h
 * @brief SKETCH: offset -> Register<Word> dispatch table built on reg_access.h.
 *
 * This is a *proposal sketch*, not yet wired into any peripheral. It closes the
 * biggest remaining gap between the in-house `regmodel` micro-library and a full
 * framework like CSML: the memory-map / address-decode / dispatch plumbing that
 * every hand-written model currently re-implements as a `reg_read`/`reg_write`
 * switch.
 *
 * With a RegisterMap:
 *   - a plain RW/RO/WO register needs **zero** decode code beyond one `add()`
 *     line (its mask contract lives in the `Register<Word>` it points at);
 *   - registers with side effects install `on_read`/`on_write` callbacks on
 *     their `Register<Word>` (W1C, FIFO ports, self-clearing bits) and still
 *     dispatch through the same table;
 *   - the whole register file is enumerable (`for_each`) for debug dumps and
 *     back-door access — closing the introspection gap too.
 *
 * What it deliberately does NOT do (kept in the model, where it belongs):
 *   - bus-level concerns: alignment/size checks, TLM response codes, DMI, and
 *     genuinely banked decode (e.g. UART DLAB remapping 0x00/0x04). Those stay
 *     in `b_transport`; the map is just the offset->register lookup underneath.
 *
 * Dependency-free beyond `reg_access.h` + a couple of STL containers.
 *
 * @code
 *   regmodel::RegisterMap<uint32_t> map;
 *   regmodel::Register32 scr{0xFFFFFFFF, 0xFFFFFFFF, 0};
 *   regmodel::Register32 intr{W1C_MASK, W1C_MASK, 0};
 *   intr.on_write([](uint32_t cur, uint32_t in){ return regmodel::apply_w1c(cur, in, W1C_MASK); });
 *   map.add(0x00, "SCR", scr)
 *      .add(0x04, "INTR_STATUS", intr);
 *
 *   // in b_transport, after alignment/size checks:
 *   uint32_t v;
 *   if (!map.read(off, v))  { gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE); return; }
 *   ...
 *   if (!map.write(off, data)) { ... }
 *
 *   // debug dump:
 *   map.for_each([&](const auto& e){
 *       os << e.name << " @0x" << std::hex << e.offset << " = 0x" << e.reg->raw() << "\n";
 *   });
 * @endcode
 */

#pragma once

#include "reg_access.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

namespace regmodel {

/// Offset-keyed dispatch table over a set of `Register<Word>` objects.
/// The map does not own the registers; they live as members of the model so
/// hardware-side logic can still touch them via `raw()/set_raw()`.
template <typename Word>
class RegisterMap {
public:
    struct Entry {
        uint64_t        offset;
        std::string     name;
        Register<Word>* reg;
    };

    /// Register a `Register<Word>` at a byte offset. Chainable.
    RegisterMap& add(uint64_t offset, std::string name, Register<Word>& reg)
    {
        by_offset_.emplace(offset, Entry{offset, std::move(name), &reg});
        return *this;
    }

    /// Software read via the register's access contract (mask or read callback).
    /// @return false if @p offset is not registered (caller issues a bus error).
    bool read(uint64_t offset, Word& out) const
    {
        const auto it = by_offset_.find(offset);
        if (it == by_offset_.end()) return false;
        out = it->second.reg->read();
        return true;
    }

    /// Software write via the register's access contract (mask or write callback).
    /// @return false if @p offset is not registered.
    bool write(uint64_t offset, Word data)
    {
        const auto it = by_offset_.find(offset);
        if (it == by_offset_.end()) return false;
        it->second.reg->write(data);
        return true;
    }

    /// Side-effect-free lookup (nullptr on miss) for back-door / hardware access.
    Register<Word>*       find(uint64_t offset)
    {
        const auto it = by_offset_.find(offset);
        return it == by_offset_.end() ? nullptr : it->second.reg;
    }
    const Register<Word>* find(uint64_t offset) const
    {
        const auto it = by_offset_.find(offset);
        return it == by_offset_.end() ? nullptr : it->second.reg;
    }

    /// Enumerate all registers in ascending-offset order (debug dumps, resets).
    /// @p fn is invoked with `const Entry&`.
    template <typename Fn>
    void for_each(Fn&& fn) const
    {
        for (const auto& kv : by_offset_) fn(kv.second);
    }

    std::size_t size()  const { return by_offset_.size(); }
    bool        empty() const { return by_offset_.empty(); }

private:
    // std::map => deterministic, offset-ordered iteration for stable dumps.
    std::map<uint64_t, Entry> by_offset_;
};

// Convenience aliases matching reg_access.h.
using RegisterMap8  = RegisterMap<uint8_t>;
using RegisterMap16 = RegisterMap<uint16_t>;
using RegisterMap32 = RegisterMap<uint32_t>;
using RegisterMap64 = RegisterMap<uint64_t>;

} // namespace regmodel
