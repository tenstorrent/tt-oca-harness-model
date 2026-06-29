#include "sep_status_report.h"

#include <fstream>
#include <string>
#include <utility>

SepStatusReport::SepStatusReport(sc_core::sc_module_name name)
    : sc_core::sc_module(name),
      verbosity("verbosity", CSML_DEFAULT_VERBOSITY),
      enable("enable", true),
      names_tsv("names_tsv", "") {

    logger.setMaxVerbosity(verbosity.get_param_value());
    // SEP_STATUS is a *literal* source field rather than %MODULE% (which resolves to the
    // SystemC instance hierarchy and cannot be forced to a fixed token). This keeps the
    // production-status lines visually consistent with SIM_OUT / internal VP status and
    // reliably greppable.
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [SEP_STATUS] - %MESSAGE%");
    logger.setFunctionTrace(false);

    decoder_.set_enabled(enable.get_param_value());
    decoder_.set_emit([this](const std::string& line) {
        // Emit at verbosity 2 so SEP_STATUS shows at the platform default verbosity.
        // LogStream appends the newline; do not add std::endl here.
        CSML_INFO(2, logger) << line;
    });

    load_names();
}

void SepStatusReport::load_names() {
    // Relative paths resolve against the config-file directory: main.cpp sets the CWD to
    // the .ini's directory before constructing the platform (same convention as `targets`).
    const std::string path = names_tsv.get_param_value();
    if (path.empty()) {
        CSML_WARN(1, logger)
            << "no names_tsv configured; status codes will show SEP_MSG_UNKNOWN";
        return;
    }
    std::ifstream f(path);
    if (!f) {
        CSML_WARN(1, logger)
            << "cannot open names_tsv '" << path
            << "'; status codes will show SEP_MSG_UNKNOWN";
        return;
    }
    auto map = sep_status_report::StatusDecoder::parse_tsv(f);
    if (map.empty()) {
        CSML_WARN(1, logger)
            << "names_tsv '" << path << "' yielded no entries; codes will show SEP_MSG_UNKNOWN";
    }
    decoder_.set_names(std::move(map));
}
