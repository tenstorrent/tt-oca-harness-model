// SPDX-License-Identifier: Apache-2.0
/**
 * @file reg_access.h
 * @brief Generic, dependency-free register access-control model for SystemC/
 *        TLM peripheral models (usable by SMC, SEP, or any other subsystem).
 *
 * This header provides a shared implementation of the register access-control
 * policy (RO/WO/RW masking, W1C/woset/lock-gated semantics, optional
 * read/write side-effect callbacks) so that individual peripheral models do
 * not each re-derive the same bit-masking logic. A bug fix or new access
 * pattern added here applies to every model that uses it, instead of being
 * re-implemented (or re-generated) per file.
 *
 * It is intentionally:
 *   - **framework-free**: depends only on `<cstdint>`/`<functional>`, not on
 *     SystemC, CCI, or any external submodule. It can back a hand-written
 *     `reg_read`/`reg_write` switch, a `csml_memory`-style callback map, or a
 *     standalone model.
 *   - **width-generic**: templated on the register word type, so an 8-bit
 *     (e.g. UART 16550), 32-bit (most CSRs), or 64-bit (e.g. mailbox) register
 *     all use the same code. Convenience aliases `Register8/16/32/64` are
 *     provided.
 *   - **callback-capable**: a register may keep the default mask-merge
 *     behavior, or install read/write callbacks for side effects (FIFO ports,
 *     self-clearing bits, W1C, banked registers), mirroring the override
 *     semantics of a framework register model without the dependency.
 *
 * See `doc/register-modeling-smc-vs-sep-csml.adoc` for the design rationale
 * and the comparison against SEP's CSML-based approach.
 */

#pragma once

#include <cstdint>
#include <functional>
#include <type_traits>

namespace regmodel {

// ---------------------------------------------------------------------------
// Free functions: the semantic write patterns, as reusable building blocks.
// Use these inside an explicit `reg_write()` switch case, or inside a
// Register<Word> write callback. They are `constexpr` and width-generic.
// ---------------------------------------------------------------------------

/// Extract bit @p i of @p v as a bool.
template <typename Word>
constexpr bool bit(Word v, unsigned i) {
    static_assert(std::is_unsigned<Word>::value, "Word must be an unsigned integer");
    return ((v >> i) & Word{1}) != Word{0};
}

/// Write-1-to-clear: bits set in `data & w1c_mask` are cleared in `reg`.
/// Matches RTL `sw = rw1c` / `sw = w1c` fields (e.g. *_STATUS interrupt regs).
template <typename Word>
constexpr Word apply_w1c(Word reg, Word data, Word w1c_mask) {
    return reg & ~(data & w1c_mask);
}

/// Write-1-to-set, sticky: bits set in `data` are OR'd into `reg` and never
/// cleared by software (only by reset). Matches RTL `sw = woset` lock regs.
template <typename Word>
constexpr Word apply_woset(Word reg, Word data) {
    return reg | data;
}

/// Lock-gated read-modify-write: bits set in `lock` are frozen (retain their
/// current value); all other bits take the new `data`. Matches RTL registers
/// whose write mask is computed as `~lock` at runtime.
template <typename Word>
constexpr Word apply_lock_gated(Word reg, Word data, Word lock) {
    const Word wmask = ~lock;
    return (data & wmask) | (reg & ~wmask);
}

/// Mask-and-merge write: `write_mask` bits take `data`, others keep `reg`.
/// This is the plain RW/RO/WO default (RO => write_mask==0, WO handled on read).
template <typename Word>
constexpr Word apply_write_mask(Word reg, Word data, Word write_mask) {
    return (data & write_mask) | (reg & ~write_mask);
}

// ---------------------------------------------------------------------------
// Register<Word>: a register whose access contract (read_mask, write_mask,
// reset_value) is fixed at construction and enforced on every access.
//
// Unlike a bare integer field decoded by hand, a Register<Word> cannot be
// read or written by software without going through read()/write(), so the
// RO/WO/RW contract is structural rather than a convention re-applied at every
// call site. For registers whose behavior is more than mask-merge (W1C, FIFO
// ports, self-clearing bits, banking), install a read/write callback instead
// of dropping to an ad-hoc switch case.
// ---------------------------------------------------------------------------
template <typename Word>
class Register {
    static_assert(std::is_unsigned<Word>::value, "Word must be an unsigned integer");

public:
    /// A write callback receives (current stored value, incoming data) and
    /// returns the new value to store. Use the free functions above to build
    /// it, e.g. `[](Word cur, Word in){ return apply_w1c(cur, in, MASK); }`.
    using WriteFn = std::function<Word(Word /*current*/, Word /*incoming*/)>;
    /// A read callback receives the stored value and returns what software
    /// sees (e.g. for read-side-effect or hardware-computed registers).
    using ReadFn  = std::function<Word(Word /*stored*/)>;

    constexpr Register(Word read_mask, Word write_mask, Word reset_value)
        : read_mask_(read_mask), write_mask_(write_mask), value_(reset_value) {}

    /// Software read: applies the read callback if set, else masks by read_mask.
    Word read() const {
        return on_read_ ? on_read_(value_) : (value_ & read_mask_);
    }

    /// Software write: applies the write callback if set, else mask-merges by
    /// write_mask.
    void write(Word data) {
        value_ = on_write_ ? on_write_(value_, data)
                           : apply_write_mask(value_, data, write_mask_);
    }

    /// Install a write callback (side effects / W1C / woset / banking).
    Register& on_write(WriteFn fn) { on_write_ = std::move(fn); return *this; }
    /// Install a read callback (read side effects / hardware-computed value).
    Register& on_read(ReadFn fn) { on_read_ = std::move(fn); return *this; }

    /// Reset the backing storage (e.g. on a soft reset).
    void reset(Word reset_value) { value_ = reset_value; }

    /// Full backing-storage value, bypassing masks/callbacks -- for
    /// hardware-side updates (a status bit driven by internal logic) and for
    /// debug/backdoor access.
    constexpr Word raw() const { return value_; }
    constexpr void set_raw(Word v) { value_ = v; }

    constexpr Word read_mask() const { return read_mask_; }
    constexpr Word write_mask() const { return write_mask_; }

private:
    Word read_mask_;
    Word write_mask_;
    Word value_;
    WriteFn on_write_{};
    ReadFn  on_read_{};
};

// Convenience aliases for the common register widths.
using Register8  = Register<uint8_t>;
using Register16 = Register<uint16_t>;
using Register32 = Register<uint32_t>;
using Register64 = Register<uint64_t>;

} // namespace regmodel
