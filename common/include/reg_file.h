// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file reg_file.h
 * @brief First-party TLM register-file types for SEP (and any other) models.
 *
 * Provides Memory / Reg / Bitfield / RegVector / Reg2D so peripherals that
 * were authored against the external CSML class-per-register shape can keep
 * that structure while depending only on this in-repo library. Mask-merge
 * writes go through regmodel::apply_write_mask from reg_access.h.
 *
 * New SMC-style models should prefer Register<Word> + RegisterMap from
 * reg_access.h / reg_map.h instead of these types.
 */

#pragma once

#include "reg_access.h"

#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_target_socket.h>
#include <vector>

namespace regmodel {

inline std::string form_report_string(std::string arg1) { return arg1; }

template <typename T>
inline std::string form_report_string(T arg) {
    return std::to_string(arg);
}

inline std::string form_report_string(const char* arg) {
    return arg ? std::string(arg) : std::string();
}

inline std::string form_report_string(const char* arg1, const char* arg2) {
    return form_report_string(arg1) + form_report_string(arg2);
}

inline std::string form_report_string(const std::string& arg1, const char* arg2) {
    return arg1 + form_report_string(arg2);
}

template <class... va_args>
std::string form_report_string(std::string arg1, va_args... args);

template <class T, class... va_args>
inline std::string form_report_string(T arg1, va_args... args) {
    return form_report_string(arg1) + form_report_string(args...);
}

template <class... va_args>
inline std::string form_report_string(std::string arg1, va_args... args) {
    return arg1 + form_report_string(args...);
}

} // namespace regmodel

#define REG_REPORT(report_type, msg_type, ...) \
    SC_REPORT_##report_type(msg_type, regmodel::form_report_string(__VA_ARGS__).c_str())

namespace regmodel {

template <unsigned int>
struct Word;

template <>
struct Word<32> {
    unsigned int word;
    typedef unsigned int wordtype;
    typedef unsigned int& reftype;
};

template <>
struct Word<16> {
    unsigned short int word;
    typedef unsigned short int wordtype;
    typedef unsigned short int& reftype;
};

template <>
struct Word<8> {
    unsigned char word;
    typedef unsigned char wordtype;
    typedef unsigned char& reftype;
};

template <>
struct Word<64> {
    unsigned long long word;
    typedef unsigned long long wordtype;
    typedef unsigned long long& reftype;
};

template <unsigned int N>
struct Memory {
    typedef typename Word<N>::wordtype DT;
    std::function<void(unsigned int, DT)> write_transport_function;
    std::function<void(unsigned int, DT&)> read_transport_function;

    std::map<unsigned int, std::function<bool(DT)>> write_callbacks;
    std::map<unsigned int, std::function<bool(DT&)>> read_callbacks;
    std::multimap<unsigned int, std::function<bool()>> post_write_callbacks;
    std::map<unsigned int, std::function<bool(DT, uint8_t)>> write_callbacks_with_be;

    Memory(std::string memory_name_, size_t memory_size_)
        : memory_name(std::move(memory_name_)), memory_block(memory_size_) {
        std::fill(memory_block.begin(), memory_block.end(), DT{0});
    }

    void bind_to_socket(tlm_utils::simple_target_socket<Memory<N>, 32>& sock) {
        sock.register_b_transport(this, &Memory<N>::b_transport);
        sock.register_transport_dbg(this, &Memory<N>::transport_dbg);
    }

    /// TLM-2.0 well-formedness gate, run before any pointer is dereferenced.
    ///
    /// Returns TLM_OK_RESPONSE when the payload is safe to service, otherwise
    /// the status to report back. Without this the decode path indexes
    /// `get_data_ptr()` unconditionally, so a null pointer is a segfault and a
    /// zero-length or streaming burst is silently mis-serviced.
    ///
    /// Address decode is deliberately *not* checked here: an access outside the
    /// populated register map keeps its existing "reserved location" behaviour
    /// (warn, read as zero, drop the write) so that firmware and peripherals
    /// relying on sparse windows are unaffected.
    static tlm::tlm_response_status validate(const tlm::tlm_generic_payload& trans) {
        const tlm::tlm_command cmd = trans.get_command();
        if (cmd != tlm::TLM_READ_COMMAND && cmd != tlm::TLM_WRITE_COMMAND) {
            // TLM_IGNORE_COMMAND carries no data phase; servicing it as a read
            // or a write would invent a transfer the initiator never asked for.
            return tlm::TLM_COMMAND_ERROR_RESPONSE;
        }

        const unsigned int len = trans.get_data_length();
        if (len == 0) return tlm::TLM_BURST_ERROR_RESPONSE;
        if (trans.get_data_ptr() == nullptr) return tlm::TLM_GENERIC_ERROR_RESPONSE;

        // streaming_width < len is a legal streaming burst that this register
        // file does not implement; 0 is illegal outright. Either way the honest
        // answer is a burst error rather than a linear transfer pretending to
        // have honoured the request.
        if (trans.get_streaming_width() < len) return tlm::TLM_BURST_ERROR_RESPONSE;

        // A non-null byte-enable pointer with length 0 is illegal TLM-2.0.
        // Treating it as "all lanes enabled" would write bytes the initiator
        // did not mark.
        if (trans.get_byte_enable_ptr() != nullptr &&
            trans.get_byte_enable_length() == 0)
            return tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE;

        return tlm::TLM_OK_RESPONSE;
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& /*delay*/) {
        const tlm::tlm_response_status vs = validate(trans);
        if (vs != tlm::TLM_OK_RESPONSE) {
            trans.set_response_status(vs);
            return;
        }
        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            read_registers(trans, false);
        } else {
            write_registers(trans, false);
        }
        trans.set_dmi_allowed(false);
        // An in-window hole stays TLM_OK and reads as zero. Whether a reserved
        // offset should instead be TLM_ADDRESS_ERROR is an open RDL question
        // (INTERNAL_REVIEW_OPEN_QUESTIONS A1), not something this path decides.
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans) {
        const tlm::tlm_response_status vs = validate(trans);
        if (vs != tlm::TLM_OK_RESPONSE) {
            trans.set_response_status(vs);
            return 0;  // TLM-2.0: number of bytes actually transferred.
        }
        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            read_registers(trans, true);
        } else {
            write_registers(trans, true);
        }
        // Same reserved-location policy as b_transport: an in-window hole is
        // still a completed transfer (A1).
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return trans.get_data_length();
    }

    void register_write_callback(std::function<bool(DT)> fn, unsigned int offset) {
        write_callbacks[offset] = std::move(fn);
    }

    void register_read_callback(std::function<bool(DT&)> fn, unsigned int offset) {
        read_callbacks[offset] = std::move(fn);
    }

    void register_post_write_callback(std::function<bool()> fn, unsigned int offset) {
        post_write_callbacks.insert({offset, std::move(fn)});
    }

    void register_write_callback_with_be(std::function<bool(DT, uint8_t)> fn,
                                         unsigned int offset) {
        write_callbacks_with_be[offset] = std::move(fn);
    }

    void run_post_write(unsigned int word_offset) {
        auto range = post_write_callbacks.equal_range(word_offset);
        for (auto it = range.first; it != range.second; ++it) {
            try {
                if (!it->second()) {
                    REG_REPORT(INFO, "POST_WRITE", "Post write failed");
                }
            } catch (const std::exception&) {
                REG_REPORT(WARNING, "MODEL_ERROR",
                           "Post-write callback not properly registered for offset ",
                           word_offset * sizeof(DT));
            }
        }
    }

    void write_registers(tlm::tlm_generic_payload& trans, bool /*is_debug*/) {
        const sc_dt::uint64 adr = trans.get_address();
        unsigned char* ptr = trans.get_data_ptr();
        unsigned char* be_ptr = trans.get_byte_enable_ptr();
        unsigned int remaining_bytes = trans.get_data_length();
        // TLM-2.0 repeats a byte-enable array shorter than the data across the
        // transfer. Indexing it linearly reads off the end of the buffer.
        const unsigned int be_len = trans.get_byte_enable_length();

        unsigned int byte_offset_in_word = static_cast<unsigned int>(adr % sizeof(DT));
        // Kept 64-bit: truncating to unsigned int lets a high address alias onto
        // a real register (0x1'0000'0008 would decode as word 1).
        sc_dt::uint64 word_offset = adr / sizeof(DT);
        unsigned int data_index = 0;

        while (remaining_bytes > 0) {
            const unsigned int bytes_in_this_word = std::min(
                static_cast<unsigned int>(sizeof(DT)) - byte_offset_in_word,
                remaining_bytes);

            if (word_offset >= memory_block.size()) {
                // Beyond the map: reserved, exactly as before. Reached without
                // narrowing, so a high address can no longer alias a real word.
                REG_REPORT(WARNING, "WRITE_ERROR",
                           "RESERVED LOCATION ", word_offset * sizeof(DT));
                remaining_bytes -= bytes_in_this_word;
                data_index += bytes_in_this_word;
                ++word_offset;
                byte_offset_in_word = 0;
                continue;
            }
            const unsigned int wo = static_cast<unsigned int>(word_offset);

            DT write_value = 0;
            for (unsigned int i = 0; i < bytes_in_this_word; ++i) {
                const unsigned int bit_position = (byte_offset_in_word + i) * 8;
                write_value |= (static_cast<DT>(ptr[data_index + i]) << bit_position);
            }

            uint8_t byte_enable = 0;
            if (be_ptr != nullptr && be_len != 0) {
                for (unsigned int i = 0; i < bytes_in_this_word; ++i) {
                    if (be_ptr[(data_index + i) % be_len] != 0) {
                        byte_enable |= static_cast<uint8_t>(1u << (byte_offset_in_word + i));
                    }
                }
            } else {
                for (unsigned int i = 0; i < bytes_in_this_word; ++i) {
                    byte_enable |= static_cast<uint8_t>(1u << (byte_offset_in_word + i));
                }
            }

            bool write_status = false;
            if (write_callbacks_with_be.count(wo) != 0) {
                try {
                    write_status = write_callbacks_with_be[wo](write_value, byte_enable);
                } catch (const std::exception&) {
                    REG_REPORT(WARNING, "MODEL_ERROR",
                               "Write callback with byte-enable not properly registered for offset ",
                               wo * sizeof(DT));
                }
                if (write_status) run_post_write(wo);
            } else if (write_callbacks.count(wo) != 0) {
                try {
                    DT callback_value = write_value;
                    if (sizeof(DT) > 4u && bytes_in_this_word < static_cast<unsigned int>(sizeof(DT))) {
                        DT be_mask = 0;
                        for (unsigned int b = 0; b < static_cast<unsigned int>(sizeof(DT)); ++b) {
                            if (byte_enable & (1u << b))
                                be_mask |= (static_cast<DT>(0xFF) << (b * 8));
                        }
                        callback_value = (write_value & be_mask) | (memory_block[wo] & ~be_mask);
                    }
                    write_status = write_callbacks[wo](callback_value);
                } catch (const std::exception&) {
                    REG_REPORT(WARNING, "MODEL_ERROR",
                               "Write callback not properly registered for offset ",
                               wo * sizeof(DT));
                }
                if (write_status) run_post_write(wo);
            } else {
                REG_REPORT(WARNING, "WRITE_ERROR",
                           "RESERVED LOCATION ", wo * sizeof(DT));
            }

            remaining_bytes -= bytes_in_this_word;
            data_index += bytes_in_this_word;
            word_offset++;
            byte_offset_in_word = 0;
        }
    }

    void read_registers(tlm::tlm_generic_payload& trans, bool /*is_debug*/) {
        const sc_dt::uint64 adr = trans.get_address();
        unsigned char* ptr = trans.get_data_ptr();
        unsigned int remaining_bytes = trans.get_data_length();

        unsigned int byte_offset_in_word = static_cast<unsigned int>(adr % sizeof(DT));
        // 64-bit for the same reason as the write path: narrowing here would let
        // an address above the map alias onto a real register.
        sc_dt::uint64 word_offset64 = adr / sizeof(DT);
        unsigned int data_index = 0;

        while (remaining_bytes > 0) {
            const unsigned int bytes_in_this_word = std::min(
                static_cast<unsigned int>(sizeof(DT)) - byte_offset_in_word,
                remaining_bytes);

            const bool in_map = word_offset64 < memory_block.size();
            const unsigned int word_offset =
                in_map ? static_cast<unsigned int>(word_offset64) : 0u;

            if (in_map && read_callbacks.count(word_offset) != 0) {
                DT read_value = 0;
                try {
                    (void)read_callbacks[word_offset](read_value);
                } catch (const std::exception&) {
                    REG_REPORT(WARNING, "MODEL_ERROR",
                               "Read callback not properly registered for offset ",
                               word_offset * sizeof(DT));
                }
                for (unsigned int i = 0; i < bytes_in_this_word; ++i) {
                    const unsigned int bit_position = (byte_offset_in_word + i) * 8;
                    ptr[data_index + i] = static_cast<unsigned char>((read_value >> bit_position) & 0xFF);
                }
            } else if (in_map) {
                // Hole in the typed map: the backing word still exists (fuse
                // shadows, sparse windows). Return it so a TLM read is defined;
                // CSML left the initiator buffer untouched, which showed up as 0
                // in the ISS and hid preload data at reserved offsets.
                const DT read_value = memory_block[word_offset];
                for (unsigned int i = 0; i < bytes_in_this_word; ++i) {
                    const unsigned int bit_position = (byte_offset_in_word + i) * 8;
                    ptr[data_index + i] = static_cast<unsigned char>((read_value >> bit_position) & 0xFF);
                }
            } else {
                REG_REPORT(WARNING, "READ_ERROR",
                           "RESERVED LOCATION ", word_offset64 * sizeof(DT));
                for (unsigned int i = 0; i < bytes_in_this_word; ++i)
                    ptr[data_index + i] = 0;
            }

            remaining_bytes -= bytes_in_this_word;
            data_index += bytes_in_this_word;
            word_offset64++;
            byte_offset_in_word = 0;
        }
    }

    unsigned int get_size() { return static_cast<unsigned int>(memory_block.size()); }

    std::string memory_name;
    std::vector<DT> memory_block;
};

template <unsigned int N>
class Reg {
public:
    typedef Memory<N> memory_type;
    typedef typename Word<N>::wordtype DT;
    typedef typename Word<N>::reftype RT;

protected:
    Reg(std::string name, memory_type& memory, unsigned int offset_,
        DT read_bit_mask_ = 0, DT write_bit_mask_ = 0, DT reset_value_ = 0)
        : register_name(std::move(name)),
          word_ref(memory.memory_block[offset_]),
          read_bit_mask(read_bit_mask_),
          write_bit_mask(write_bit_mask_),
          reset_value(reset_value_),
          offset(offset_) {
        memory.register_read_callback(
            std::bind(&Reg<N>::handle_read, this, std::placeholders::_1, this->read_bit_mask),
            offset);
        memory.register_write_callback(
            std::bind(&Reg<N>::handle_write, this, std::placeholders::_1, this->write_bit_mask),
            offset);
        Reg::reset();
    }
    Reg(const Reg&) = delete;
    Reg(Reg&&) = delete;
    Reg& operator=(Reg&&) = delete;
    virtual ~Reg() = default;

protected:
    std::string register_name;
    RT word_ref;

public:
    DT read_bit_mask;
    DT write_bit_mask;
    DT reset_value;
    unsigned int offset;

    RT get_word_reference() { return word_ref; }

    operator DT() const { return word_ref; }
    Reg& operator=(DT value) {
        word_ref = value;
        return *this;
    }
    Reg& operator=(const Reg& r) {
        word_ref = r.word_ref;
        return *this;
    }
    Reg& operator+=(DT value) { word_ref += value; return *this; }
    Reg& operator-=(DT value) { word_ref -= value; return *this; }
    Reg& operator/=(DT value) { word_ref /= value; return *this; }
    Reg& operator*=(DT value) { word_ref *= value; return *this; }
    Reg& operator%=(DT value) { word_ref %= value; return *this; }
    Reg& operator^=(DT value) { word_ref ^= value; return *this; }
    Reg& operator&=(DT value) { word_ref &= value; return *this; }
    Reg& operator|=(DT value) { word_ref |= value; return *this; }
    Reg& operator>>=(DT value) { word_ref >>= value; return *this; }
    Reg& operator<<=(DT value) { word_ref <<= value; return *this; }
    Reg& operator--() { word_ref = word_ref - 1; return *this; }
    DT operator--(int) {
        DT temp = word_ref;
        word_ref--;
        return temp;
    }
    Reg& operator++() { word_ref = word_ref + 1; return *this; }
    DT operator++(int) {
        DT temp = word_ref;
        word_ref++;
        return temp;
    }

    virtual void reset() { word_ref = reset_value; }

    virtual bool handle_write(DT value, DT bitmask) {
        word_ref = apply_write_mask(word_ref, value, bitmask);
        return true;
    }
    virtual bool handle_read(DT& value, DT bitmask) {
        value = word_ref & bitmask;
        return true;
    }
    virtual bool handle_read_restriction_error(DT& value) {
        value = 0;
        REG_REPORT(WARNING, "READ_ERROR", this->register_name, " is write only register");
        return false;
    }
    virtual bool handle_write_restriction_error(DT /*value*/) {
        REG_REPORT(WARNING, "WRITE_ERROR", this->register_name, " is read only register");
        return false;
    }

    void set_read_write_restrictions(memory_type& memory) {
        if (read_bit_mask == 0) {
            memory.register_read_callback(
                std::bind(&Reg<N>::handle_read_restriction_error, this, std::placeholders::_1),
                offset);
        }
        if (write_bit_mask == 0) {
            memory.register_write_callback(
                std::bind(&Reg<N>::handle_write_restriction_error, this, std::placeholders::_1),
                offset);
        }
    }
};

template <unsigned int N>
class Bitfield {
    typedef typename Word<N>::wordtype DT;
    using RT = Reg<N>&;

public:
    Bitfield(std::string name, RT reg_reference_, unsigned int startbit_,
             unsigned int number_of_bits_)
        : bitfield_name(std::move(name)),
          word_reference(reg_reference_.get_word_reference()),
          startbit(startbit_),
          num_of_bits(number_of_bits_) {
        bit_mask = (static_cast<DT>(-1) >> (N - num_of_bits)) << startbit;
    }
    Bitfield(Bitfield&&) = delete;
    Bitfield(const Bitfield&) = delete;
    Bitfield& operator=(Bitfield&&) = delete;
    ~Bitfield() = default;

    DT get() const { return (word_reference & bit_mask) >> startbit; }
    void set_bits_to_register(DT value, unsigned int bit_offset, unsigned int /*size*/) {
        word_reference = (word_reference & ~bit_mask) | ((value << bit_offset) & bit_mask);
    }
    void put(DT value) { set_bits_to_register(value, startbit, num_of_bits); }
    void put(const Bitfield& b) { put(static_cast<DT>(b)); }

    operator DT() const { return get(); }
    Bitfield& operator=(DT value) { put(value); return *this; }
    Bitfield& operator=(const Bitfield& b) { put(b); return *this; }
    Bitfield& operator+=(DT value) { put(get() + value); return *this; }
    Bitfield& operator-=(DT value) { put(get() - value); return *this; }
    Bitfield& operator/=(DT value) { put(get() / value); return *this; }
    Bitfield& operator*=(DT value) { put(get() * value); return *this; }
    Bitfield& operator%=(DT value) { put(get() % value); return *this; }
    Bitfield& operator^=(DT value) { put(get() ^ value); return *this; }
    Bitfield& operator&=(DT value) { put(get() & value); return *this; }
    Bitfield& operator|=(DT value) { put(get() | value); return *this; }
    Bitfield& operator<<=(DT value) { put(get() << value); return *this; }
    Bitfield& operator>>=(DT value) { put(get() >> value); return *this; }
    Bitfield& operator--() { put(get() - 1); return *this; }
    DT operator--(int) {
        DT temp = get();
        put(temp - 1);
        return temp;
    }
    Bitfield& operator++() { put(get() + 1); return *this; }
    DT operator++(int) {
        DT temp = get();
        put(temp + 1);
        return temp;
    }

private:
    std::string bitfield_name;
    typename Word<N>::reftype word_reference;
    unsigned int startbit;
    unsigned int num_of_bits;
    DT bit_mask;
};

template <class T, unsigned int Quantity>
class RegVector {
public:
    RegVector(std::string name, typename T::memory_type& mem, unsigned int offset,
              unsigned int spacing) {
        for (unsigned int i = 0; i < Quantity; ++i)
            registers[i] = std::make_unique<T>(name + "_" + std::to_string(i), mem,
                                               offset + i * spacing);
    }

    T& operator[](unsigned int num) {
        if (num < Quantity) return *registers[num];
        REG_REPORT(ERROR, "MODEL_ERROR", "Access limit of ", Quantity - 1,
                   " Requested access to ", num);
        throw std::out_of_range("RegVector out of bound access");
    }
    const T& operator[](unsigned int num) const {
        if (num < Quantity) return *registers[num];
        REG_REPORT(ERROR, "MODEL_ERROR", "Access limit of ", Quantity - 1,
                   ". Requested access to ", num);
        throw std::out_of_range("RegVector out of bound access");
    }
    constexpr size_t size() const { return Quantity; }
    std::array<std::unique_ptr<T>, Quantity> registers;
};

template <class T, unsigned int Quantity1, unsigned int Quantity2>
class Reg2D {
public:
    Reg2D(std::string name, typename T::memory_type& mem, unsigned int offset,
          unsigned int stride, unsigned int spacing) {
        for (unsigned int i = 0; i < Quantity1; ++i)
            rows[i] = std::make_unique<RegVector<T, Quantity2>>(
                name + "_" + std::to_string(i), mem, offset + i * stride, spacing);
    }

    RegVector<T, Quantity2>& operator[](unsigned int num) {
        if (num < Quantity1) return *rows[num];
        REG_REPORT(ERROR, "MODEL_ERROR", "Access limit of ", Quantity1 - 1,
                   ". Requested access to ", num);
        throw std::out_of_range("Reg2D out of bound access");
    }
    const RegVector<T, Quantity2>& operator[](unsigned int num) const {
        if (num < Quantity1) return *rows[num];
        REG_REPORT(ERROR, "MODEL_ERROR", "Access limit of ", Quantity1 - 1,
                   ". Requested access to ", num);
        throw std::out_of_range("Reg2D out of bound access");
    }
    constexpr size_t size() const { return Quantity1 * Quantity2; }
    constexpr size_t size_row() const { return Quantity1; }
    constexpr size_t size_column() const { return Quantity2; }
    std::array<std::unique_ptr<RegVector<T, Quantity2>>, Quantity1> rows;
};

template <unsigned int N>
class RegisterGroup {
public:
    typedef Memory<N> memory_type;
    typedef typename Word<N>::wordtype DT;
};

} // namespace regmodel
