// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

#include "sep_memory.h"

#include <cci/utils/consuming_broker.h>
#include <tlm_utils/simple_initiator_socket.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>

namespace {
unsigned failures;

#define CHECK(expr) do { \
    if (!(expr)) { \
        std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": " #expr "\n"; \
        ++failures; \
    } \
} while (false)

class bench : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<bench> ram_socket{"ram_socket"};
    tlm_utils::simple_initiator_socket<bench> rom_socket{"rom_socket"};
    SEPMemory ram{"ram", false};
    SEPMemory rom{"rom", true};

    SC_CTOR(bench)
    {
        ram_socket.bind(ram.tsock);
        rom_socket.bind(rom.tsock);
    }

    tlm::tlm_response_status transfer(tlm_utils::simple_initiator_socket<bench>& socket,
                                      tlm::tlm_command command, uint64_t address,
                                      void* data, unsigned length,
                                      unsigned char* byte_enables = nullptr,
                                      unsigned byte_enable_length = 0,
                                      sc_core::sc_time* observed_delay = nullptr)
    {
        tlm::tlm_generic_payload gp;
        gp.set_command(command);
        gp.set_address(address);
        gp.set_data_ptr(static_cast<unsigned char*>(data));
        gp.set_data_length(length);
        gp.set_streaming_width(length);
        gp.set_byte_enable_ptr(byte_enables);
        gp.set_byte_enable_length(byte_enable_length);
        sc_core::sc_time delay(3, sc_core::SC_NS);
        socket->b_transport(gp, delay);
        if (observed_delay) {
            *observed_delay = delay;
        }
        return gp.get_response_status();
    }
};
} // namespace

int sc_main(int, char**)
{
    cci::cci_register_broker(new cci_utils::consuming_broker("GlobalBroker"));
    bench tb{"tb"};
    tb.ram.logger.setMaxVerbosity(5);
    tb.rom.logger.setMaxVerbosity(5);

    std::array<unsigned char, 4> write{0x11, 0x22, 0x33, 0x44};
    std::array<unsigned char, 4> read{};
    sc_core::sc_time delay;
    CHECK(tb.transfer(tb.ram_socket, tlm::TLM_WRITE_COMMAND, 0x100, write.data(),
                      write.size(), nullptr, 0, &delay) == tlm::TLM_OK_RESPONSE);
    CHECK(delay == sc_core::sc_time(13, sc_core::SC_NS));
    CHECK(tb.transfer(tb.ram_socket, tlm::TLM_READ_COMMAND, 0x100, read.data(),
                      read.size()) == tlm::TLM_OK_RESPONSE);
    CHECK(read == write);

    unsigned char enables[] = {TLM_BYTE_ENABLED, TLM_BYTE_DISABLED};
    std::array<unsigned char, 4> patch{0xAA, 0xBB, 0xCC, 0xDD};
    CHECK(tb.transfer(tb.ram_socket, tlm::TLM_WRITE_COMMAND, 0x100, patch.data(),
                      patch.size(), enables, 2) == tlm::TLM_OK_RESPONSE);
    CHECK(tb.transfer(tb.ram_socket, tlm::TLM_READ_COMMAND, 0x100, read.data(),
                      read.size()) == tlm::TLM_OK_RESPONSE);
    CHECK((read == std::array<unsigned char, 4>{0xAA, 0x22, 0xCC, 0x44}));

    unsigned char dummy = 0;
    CHECK(tb.transfer(tb.ram_socket, tlm::TLM_IGNORE_COMMAND, 0, &dummy, 1)
          == tlm::TLM_COMMAND_ERROR_RESPONSE);
    CHECK(tb.transfer(tb.ram_socket, tlm::TLM_READ_COMMAND, 0, nullptr, 1)
          == tlm::TLM_GENERIC_ERROR_RESPONSE);
    CHECK(tb.transfer(tb.ram_socket, tlm::TLM_WRITE_COMMAND, 0, &dummy, 1,
                      enables, 0) == tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);

    const char initial[] = {1, 2, 3, 4};
    tb.rom.load_data(initial, 0x80, sizeof(initial));
    std::array<unsigned char, 4> rom_write{9, 9, 9, 9};
    CHECK(tb.transfer(tb.rom_socket, tlm::TLM_WRITE_COMMAND, 0x80, rom_write.data(),
                      rom_write.size()) == tlm::TLM_OK_RESPONSE);
    read.fill(0);
    CHECK(tb.transfer(tb.rom_socket, tlm::TLM_READ_COMMAND, 0x80, read.data(),
                      read.size()) == tlm::TLM_OK_RESPONSE);
    CHECK((read == std::array<unsigned char, 4>{1, 2, 3, 4}));

    tb.ram.load_zero(0x100, read.size());
    read.fill(0xFF);
    CHECK(tb.transfer(tb.ram_socket, tlm::TLM_READ_COMMAND, 0x100, read.data(),
                      read.size()) == tlm::TLM_OK_RESPONSE);
    CHECK((read == std::array<unsigned char, 4>{0, 0, 0, 0}));

    const std::string binary_path = "/tmp/sep_memory_tb.bin";
    {
        std::ofstream binary(binary_path, std::ios::binary);
        binary.write(initial, sizeof(initial));
    }
    tb.ram.load_binary_file(binary_path, 0x200);
    read.fill(0);
    CHECK(tb.transfer(tb.ram_socket, tlm::TLM_READ_COMMAND, 0x200, read.data(),
                      read.size()) == tlm::TLM_OK_RESPONSE);
    CHECK((read == std::array<unsigned char, 4>{1, 2, 3, 4}));
    std::remove(binary_path.c_str());

    tlm::tlm_generic_payload dmi_gp;
    dmi_gp.set_address(0x100);
    tlm::tlm_dmi dmi;
    CHECK(tb.ram_socket->get_direct_mem_ptr(dmi_gp, dmi));
    CHECK(dmi.is_read_allowed() && dmi.is_write_allowed());
    tlm::tlm_dmi rom_dmi;
    dmi_gp.set_address(0x80);
    CHECK(tb.rom_socket->get_direct_mem_ptr(dmi_gp, rom_dmi));
    CHECK(rom_dmi.is_read_allowed() && !rom_dmi.is_write_allowed());
    tlm::tlm_dmi missing_dmi;
    dmi_gp.set_address(0x100000);
    CHECK(!tb.ram_socket->get_direct_mem_ptr(dmi_gp, missing_dmi));

    tlm::tlm_generic_payload dbg;
    dbg.set_command(tlm::TLM_READ_COMMAND);
    dbg.set_address(0x100);
    dbg.set_data_ptr(read.data());
    dbg.set_data_length(read.size());
    CHECK(tb.ram_socket->transport_dbg(dbg) == read.size());
    dbg.set_command(tlm::TLM_WRITE_COMMAND);
    CHECK(tb.ram_socket->transport_dbg(dbg) == read.size());
    CHECK(tb.rom_socket->transport_dbg(dbg) == read.size());
    dbg.set_command(tlm::TLM_IGNORE_COMMAND);
    CHECK(tb.ram_socket->transport_dbg(dbg) == 0);
    dbg.set_command(tlm::TLM_READ_COMMAND);
    dbg.set_data_ptr(nullptr);
    CHECK(tb.ram_socket->transport_dbg(dbg) == 0);

    sc_core::sc_start(sc_core::SC_ZERO_TIME);
    std::cout << (failures == 0 ? "ALL TESTS PASSED\n" : "TESTS FAILED\n");
    return failures == 0 ? 0 : 1;
}
