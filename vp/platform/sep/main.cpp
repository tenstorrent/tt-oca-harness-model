// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include <systemc.h>
#include <filesystem>
#include <cstdlib>
#include <cstring>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include "sep_platform.hpp"
#include "reg_logger.h"
#include "tlm_quantum_policy.h"

namespace {

// Idle initiator: satisfies the BW port of the platform's inbound boundary
// target socket (sep_smn_inbound_axi) without issuing traffic — standalone
// sep-vp has no SMU crossbar master.
class idle_initiator : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<idle_initiator, 64> sock{"sock"};
    explicit idle_initiator(sc_core::sc_module_name name) : sc_core::sc_module(name) {}
};

// Accept-all stub target: terminates the platform's outbound boundary ports
// (sep_ext_to_smc_axi, sep_smn_outbound_axi) in standalone sep-vp, where there
// is no chiplet fabric to receive them.  Reads are zero-filled rather than left
// alone so a stray read cannot return whatever was in the caller's buffer.
class sink_target : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<sink_target, 64> sock{"sock"};
    explicit sink_target(sc_core::sc_module_name name) : sc_core::sc_module(name) {
        sock.register_b_transport(this, &sink_target::b_transport);
        sock.register_transport_dbg(this, &sink_target::transport_dbg);
    }
private:
    static void zero_reads(tlm::tlm_generic_payload& trans) {
        if (!trans.is_read()) return;
        unsigned char* p = trans.get_data_ptr();
        if (p && trans.get_data_length() > 0)
            std::memset(p, 0, trans.get_data_length());
    }
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
        delay = sc_core::SC_ZERO_TIME;
        zero_reads(trans);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans) {
        zero_reads(trans);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return trans.get_data_length();
    }
};

} // namespace

int sc_main(int argc, char **argv)
{
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: " << argv[0] << " CCI parameter config file [targets override]\n";
        return 1;
    }

    std::filesystem::path launchCwd = std::filesystem::current_path();

    std::string arg = argv[1];
    std::cout << "Argument received: " << arg << "\n";
    std::filesystem::path cfgPath = std::filesystem::absolute(std::filesystem::path(argv[1]));
    (void) ::setenv("SEP_VP_INI_DIR", cfgPath.parent_path().string().c_str(), 1);
    std::filesystem::current_path(cfgPath.parent_path());
    regmodel::load_config_file(cfgPath.string().c_str());

    if (argc == 3) {
        std::string targets_override(argv[2]);

        // If override is a single path token, interpret it relative to the
        // directory where the user launched the binary.
        if (targets_override.find(' ') == std::string::npos) {
            std::filesystem::path tpath(targets_override);
            if (tpath.is_relative()) {
                targets_override = (launchCwd / tpath).lexically_normal().string();
            }
        }

        std::cout << "CCI targets override from command line: " << targets_override << "\n";
        cci::cci_originator orig("sc_main");
        cci::cci_broker_handle broker(cci::cci_get_global_broker(orig));
        // regmodel::Param<vector<string>> is backed by a CCI string param (JSON-encoded).
        // The ini parser sets a single path as a plain JSON-quoted string; match that format.
        std::string targets_json = "\"" + targets_override + "\"";
        broker.set_preset_cci_value("och_sep_ss1.targets", cci::cci_value::from_json(targets_json));
    }

    std::cout << "RegLogger global log file: och_sep_ss.log\n";
    RegLogger::setGlobalLogFile("och_sep_ss.log");

    {
        cci::cci_originator orig("sc_main");
        auto broker = cci::cci_get_global_broker(orig);
        uint64_t qns = simtlm::DEFAULT_GLOBAL_QUANTUM_NS;
        const cci::cci_value v = broker.get_preset_cci_value("och_sep_ss1.globalQuantumNs");
        if (v.is_uint64())
            qns = v.get_uint64();
        simtlm::set_global_quantum_ns(qns);
    }

    och_sep_ss och_sep_ss1("och_sep_ss1");

    // Standalone sep-vp has no SMU platform: bind the chiplet boundary ports
    // idle (inbound) / to sinks (dedicated SMC window and general outbound).
    idle_initiator idle_smn_in{"idle_smn_in"};
    sink_target    sink_smc_win{"sink_smc_win"};
    sink_target    sink_smn_out{"sink_smn_out"};
    idle_smn_in.sock.bind(och_sep_ss1.sep_smn_inbound_axi);
    och_sep_ss1.sep_ext_to_smc_axi.bind(sink_smc_win.sock);
    och_sep_ss1.sep_smn_outbound_axi.bind(sink_smn_out.sock);

    sc_start();
    return 0;
}
