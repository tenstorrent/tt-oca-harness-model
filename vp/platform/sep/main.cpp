#include <systemc.h>
#include <filesystem>
#include <cstdlib>
#include "och_sep_ss.hpp"
#include "csml_logger.h"

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
    sc_start(); 
    return 0;
}
