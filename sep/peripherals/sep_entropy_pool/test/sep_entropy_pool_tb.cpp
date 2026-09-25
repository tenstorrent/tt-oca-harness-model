// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

#include "sep_entropy_pool.h"

#include <cci/utils/consuming_broker.h>
#include <cstring>
#include <deque>
#include <iostream>
#include <tlm_utils/simple_initiator_socket.h>

class mock_edn_source : public sc_core::sc_module, public edn_endpoint_if
{
public:
    sc_core::sc_export<edn_endpoint_if> endpoint{"endpoint"};

    explicit mock_edn_source(sc_core::sc_module_name name) : sc_module(name)
    {
        endpoint.bind(*this);
    }

    bool try_pop_entropy_word(unsigned endpoint_id, uint32_t& word, bool& fips) override
    {
        if (endpoint_id != sep_entropy_pool_ip::POOL_ENDPOINT_ID || words_.empty())
            return false;
        word = words_.front();
        words_.pop_front();
        fips = true;
        return true;
    }

    const sc_core::sc_event& entropy_available_event() const override
    {
        return available_;
    }

    void push(uint32_t word)
    {
        words_.push_back(word);
        available_.notify(sc_core::SC_ZERO_TIME);
    }

private:
    std::deque<uint32_t> words_;
    mutable sc_core::sc_event available_;
};

class entropy_pool_tb : public sc_core::sc_module
{
public:
    tlm_utils::simple_initiator_socket<entropy_pool_tb> init{"init"};
    sep_entropy_pool_ip dut{"dut"};
    mock_edn_source source{"source"};
    sc_core::sc_signal<bool> rst_n{"rst_n"};
    sc_core::sc_signal<bool> low{"low"};
    sc_core::sc_signal<bool> stall{"stall"};
    sc_core::sc_signal<bool> error{"error"};

    SC_HAS_PROCESS(entropy_pool_tb);
    explicit entropy_pool_tb(sc_core::sc_module_name name) : sc_module(name)
    {
        init.bind(dut.reg_socket);
        dut.entropy_source.bind(source.endpoint);
        dut.rst_ni.bind(rst_n);
        dut.pool_low_o.bind(low);
        dut.fill_stall_o.bind(stall);
        dut.pool_error_o.bind(error);
        SC_THREAD(run);
    }

    int failures() const { return failures_; }

private:
    int failures_ = 0;

    void check(bool condition, const char* message)
    {
        if (!condition) {
            ++failures_;
            std::cerr << "FAIL: " << message << '\n';
        }
    }

    tlm::tlm_response_status transact(tlm::tlm_command command,
                                      uint64_t address,
                                      void* data,
                                      unsigned length,
                                      unsigned streaming = 0,
                                      unsigned char* byte_en = nullptr,
                                      unsigned byte_en_len = 0,
                                      sc_core::sc_time* observed_delay = nullptr)
    {
        tlm::tlm_generic_payload gp;
        gp.set_command(command);
        gp.set_address(address);
        gp.set_data_ptr(static_cast<unsigned char*>(data));
        gp.set_data_length(length);
        gp.set_streaming_width(streaming);
        gp.set_byte_enable_ptr(byte_en);
        gp.set_byte_enable_length(byte_en_len);
        gp.set_dmi_allowed(true);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        init->b_transport(gp, delay);
        check(!gp.is_dmi_allowed(), "b_transport must clear stale DMI permission");
        if (observed_delay)
            *observed_delay = delay;
        return gp.get_response_status();
    }

    uint64_t read_ok(uint64_t offset)
    {
        uint64_t value = ~uint64_t{0};
        check(transact(tlm::TLM_READ_COMMAND, offset, &value, sizeof(value)) ==
                  tlm::TLM_OK_RESPONSE,
              "expected successful read");
        return value;
    }

    void run()
    {
        rst_n.write(false);
        wait(sc_core::SC_ZERO_TIME);
        check(low.read(), "empty/reset pool must assert pool_low");
        check(!stall.read() && !error.read(), "reset must clear fault outputs");
        rst_n.write(true);
        wait(sc_core::SC_ZERO_TIME);

        check(read_ok(sep_entropy_pool_ip::STATUS_OFFSET) == (uint64_t{1} << 6),
              "empty STATUS must report only pool_low");
        check(read_ok(sep_entropy_pool_ip::IRQ_CAUSE_OFFSET) == 1u,
              "empty IRQ_CAUSE must report pool_low");

        check(dut.feed_word32(0x11223344u, true), "first packed word accepted");
        check(dut.feed_word32(0x55667788u, true), "second packed word accepted");
        wait(sc_core::SC_ZERO_TIME);
        check((read_ok(sep_entropy_pool_ip::STATUS_OFFSET) & 0x3fu) == 1u,
              "two 32-bit words must create one 64-bit entry");

        uint64_t dbg_data = 0;
        tlm::tlm_generic_payload dbg;
        dbg.set_command(tlm::TLM_READ_COMMAND);
        dbg.set_address(sep_entropy_pool_ip::DATA_OFFSET);
        dbg.set_data_ptr(reinterpret_cast<unsigned char*>(&dbg_data));
        dbg.set_data_length(sizeof(dbg_data));
        dbg.set_streaming_width(sizeof(dbg_data));
        check(init->transport_dbg(dbg) == sizeof(dbg_data),
              "debug DATA read returns one word");
        check(dbg_data == 0x5566778811223344ULL,
              "packer order must match 32-to-64 RTL packing");
        check((read_ok(sep_entropy_pool_ip::STATUS_OFFSET) & 0x3fu) == 1u,
              "debug DATA read must be non-destructive");
        check(read_ok(sep_entropy_pool_ip::DATA_OFFSET) == dbg_data,
              "frontdoor DATA read returns packed value");
        wait(sc_core::SC_ZERO_TIME);
        check(dut.fifo_level() == 0u, "frontdoor DATA read pops exactly once");

        dbg.set_address(sep_entropy_pool_ip::STATUS_OFFSET);
        check(init->transport_dbg(dbg) == sizeof(dbg_data),
              "debug STATUS read succeeds");
        dbg.set_address(sep_entropy_pool_ip::IRQ_CAUSE_OFFSET);
        check(init->transport_dbg(dbg) == sizeof(dbg_data),
              "debug IRQ_CAUSE read succeeds");
        dbg.set_address(sep_entropy_pool_ip::DATA_OFFSET);
        check(init->transport_dbg(dbg) == 0u, "debug empty DATA read fails");
        dbg.set_command(tlm::TLM_WRITE_COMMAND);
        check(init->transport_dbg(dbg) == 0u, "debug writes are rejected");
        dbg.set_command(tlm::TLM_READ_COMMAND);
        dbg.set_address(0x18);
        check(init->transport_dbg(dbg) == 0u, "debug hole read is rejected");

        uint64_t value = ~uint64_t{0};
        check(transact(tlm::TLM_READ_COMMAND, sep_entropy_pool_ip::DATA_OFFSET,
                       &value, sizeof(value)) == tlm::TLM_GENERIC_ERROR_RESPONSE,
              "empty DATA read must return slave-style error");
        check(value == 0u, "empty DATA read must return zero data");

        value = 0xabcdefu;
        check(transact(tlm::TLM_WRITE_COMMAND, sep_entropy_pool_ip::STATUS_OFFSET,
                       &value, sizeof(value)) == tlm::TLM_GENERIC_ERROR_RESPONSE,
              "all writes must be rejected");
        check(dut.fifo_level() == 0u, "rejected write must not mutate pool");

        sc_core::sc_time delay;
        value = 0;
        check(transact(tlm::TLM_READ_COMMAND, sep_entropy_pool_ip::STATUS_OFFSET,
                       &value, sizeof(value), 0, nullptr, 0, &delay) ==
                  tlm::TLM_OK_RESPONSE,
              "valid delayed read succeeds");
        check(delay == sc_core::sc_time(1, sc_core::SC_NS),
              "access delay must be annotated");

        unsigned char be[8];
        std::memset(be, TLM_BYTE_ENABLED, sizeof(be));
        check(transact(tlm::TLM_READ_COMMAND, 0, &value, sizeof(value),
                       sizeof(value), be, sizeof(be)) ==
                  tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,
              "byte enables are rejected");
        check(transact(tlm::TLM_IGNORE_COMMAND, 0, &value, sizeof(value)) ==
                  tlm::TLM_COMMAND_ERROR_RESPONSE,
              "IGNORE is rejected");
        check(transact(tlm::TLM_READ_COMMAND, 0, nullptr, sizeof(value)) ==
                  tlm::TLM_GENERIC_ERROR_RESPONSE,
              "null data pointer is rejected");
        check(transact(tlm::TLM_READ_COMMAND, 0, &value, 0) ==
                  tlm::TLM_BURST_ERROR_RESPONSE,
              "zero length is rejected");
        check(transact(tlm::TLM_READ_COMMAND, 0, &value, 1) ==
                  tlm::TLM_BURST_ERROR_RESPONSE,
              "one-byte access is rejected");
        uint32_t status32 = ~uint32_t{0};
        check(transact(tlm::TLM_READ_COMMAND, 0, &status32, sizeof(status32)) ==
                  tlm::TLM_OK_RESPONSE,
              "legal 32-bit low lane is supported for RV32 firmware");
        check(status32 == (1u << 6), "32-bit STATUS returns low register lane");
        check(transact(tlm::TLM_READ_COMMAND, 0, &value, 7) ==
                  tlm::TLM_BURST_ERROR_RESPONSE,
              "width-minus-one is rejected");
        check(transact(tlm::TLM_READ_COMMAND, 0, &value, 9) ==
                  tlm::TLM_BURST_ERROR_RESPONSE,
              "width-plus-one is rejected");
        check(transact(tlm::TLM_READ_COMMAND, 0, &value, sizeof(value), 4) ==
                  tlm::TLM_BURST_ERROR_RESPONSE,
              "short streaming width is rejected");
        check(transact(tlm::TLM_READ_COMMAND, 1, &value, sizeof(value)) ==
                  tlm::TLM_ADDRESS_ERROR_RESPONSE,
              "unaligned access is rejected");
        check(transact(tlm::TLM_READ_COMMAND, 0x18, &value, sizeof(value)) ==
                  tlm::TLM_GENERIC_ERROR_RESPONSE,
              "aligned aperture hole returns slave-style error");
        check(transact(tlm::TLM_READ_COMMAND,
                       sep_entropy_pool_ip::APERTURE_SIZE - 1,
                       &value, sizeof(value)) ==
                  tlm::TLM_ADDRESS_ERROR_RESPONSE,
              "last-byte unaligned access is rejected");

        tlm::tlm_generic_payload dmi_gp;
        tlm::tlm_dmi dmi;
        check(!init->get_direct_mem_ptr(dmi_gp, dmi), "DMI must be denied");

        for (uint32_t i = 0; i < 16; ++i)
            source.push(0x80000000u + i);
        wait(sc_core::sc_time(1, sc_core::SC_NS));
        check(dut.fifo_level() == sep_entropy_pool_ip::LOW_WATERMARK,
              "EDN fills through the low watermark");
        check(!low.read() && read_ok(sep_entropy_pool_ip::IRQ_CAUSE_OFFSET) == 0u,
              "pool_low is a live level and clears at watermark");
        (void)read_ok(sep_entropy_pool_ip::DATA_OFFSET);
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);
        check(low.read(), "pool_low reasserts below watermark");
        for (unsigned i = 1; i < sep_entropy_pool_ip::LOW_WATERMARK; ++i)
            (void)read_ok(sep_entropy_pool_ip::DATA_OFFSET);

        source.push(0x01020304u);
        source.push(0x11121314u);
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);
        check(dut.fifo_level() == 1u, "bound EDN source fills pool");
        check(read_ok(sep_entropy_pool_ip::DATA_OFFSET) ==
                  0x1112131401020304ULL,
              "EDN source preserves word ordering");
        wait(sc_core::sc_time(20'479, sc_core::SC_NS));
        check(!stall.read(), "fill stall must stay low before threshold");
        wait(sc_core::sc_time(1, sc_core::SC_NS));
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);
        check(stall.read(), "fill stall asserts at LT threshold");

        source.push(0xa5a5a5a5u);
        wait(sc_core::sc_time(1, sc_core::SC_NS));
        check(!stall.read(), "forward progress clears fill stall");

        source.push(0x5a5a5a5au);
        wait(sc_core::sc_time(1, sc_core::SC_NS));
        check(dut.fifo_level() == 1u, "post-stall source resumes packing");
        rst_n.write(false);
        wait(sc_core::SC_ZERO_TIME);
        check(dut.fifo_level() == 0u, "reset scrubs queued entropy");
        check(!dut.feed_word32(0xdeadbeefu, true),
              "reset blocks entropy acceptance");
        check(low.read() && !stall.read() && !error.read(),
              "reset restores output levels");

        rst_n.write(true);
        wait(sc_core::SC_ZERO_TIME);
        check(sep_entropy_pool_ip::FIFO_DEPTH == 32u,
              "pool capacity matches RTL");
        for (unsigned i = 0; i < 64; ++i)
            check(dut.feed_word32(i, true), "pool accepts words up to capacity");
        check(dut.feed_word32(64, true), "full pool accepts first packer half");
        check(!dut.feed_word32(65, true), "full pool rejects completed entry");
        source.push(0x12345678u);
        wait(sc_core::sc_time(1, sc_core::SC_NS));
        check(dut.fifo_level() == sep_entropy_pool_ip::FIFO_DEPTH,
              "EDN source cannot overfill pool");

        if (failures_ == 0)
            std::cout << "ALL TESTS PASSED\n";
        sc_core::sc_stop();
    }
};

int sc_main(int, char**)
{
    static cci_utils::consuming_broker cci_global_broker("GlobalBroker");
    cci::cci_register_broker(cci_global_broker);
    entropy_pool_tb tb{"tb"};
    sc_core::sc_start();
    return tb.failures() == 0 ? 0 : 1;
}
