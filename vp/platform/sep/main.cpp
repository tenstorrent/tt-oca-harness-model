#include <systemc.h>
#include <filesystem>
#include <cstdlib>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include "och_sep_ss.hpp"
#include "csml_logger.h"

namespace {

// Idle initiator: satisfies the BW port of the platform's inbound boundary
// target socket (sep_smn_inbound_axi) without issuing traffic — standalone
// sep-vp has no SMU crossbar master.
class idle_initiator : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<idle_initiator, 64> sock{"sock"};
    explicit idle_initiator(sc_core::sc_module_name name) : sc_core::sc_module(name) {}
};

// Accept-all stub target: terminates the platform's outbound boundary port
// (sep_ext_to_smc_axi) in standalone sep-vp.  Never receives traffic (the
// smc_global window falls back to its internal RW store unless
// `smc_global.forward_en` is set), but the socket must be bound.
class sink_target : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<sink_target, 64> sock{"sock"};
    explicit sink_target(sc_core::sc_module_name name) : sc_core::sc_module(name) {
        sock.register_b_transport(this, &sink_target::b_transport);
    }
private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&) {
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
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
    load_config_file(cfgPath.string().c_str());

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
        // csml_param<vector<string>> is backed by a CCI string param (JSON-encoded).
        // The ini parser sets a single path as a plain JSON-quoted string; match that format.
        std::string targets_json = "\"" + targets_override + "\"";
        broker.set_preset_cci_value("och_sep_ss1.targets", cci::cci_value::from_json(targets_json));
    }

    std::cout << "CSML global log file: och_sep_ss.log\n";
    CsmlLogger::setGlobalLogFile("och_sep_ss.log");
    och_sep_ss och_sep_ss1("och_sep_ss1");

    // Standalone sep-vp has no SMU platform: bind the chiplet boundary ports
    // idle (inbound) / to a sink (outbound dedicated SMC window).
    idle_initiator idle_smn_in{"idle_smn_in"};
    sink_target    sink_smc_win{"sink_smc_win"};
    idle_smn_in.sock.bind(och_sep_ss1.sep_smn_inbound_axi);
    och_sep_ss1.sep_ext_to_smc_axi.bind(sink_smc_win.sock);

    sc_start();
    return 0;
}
