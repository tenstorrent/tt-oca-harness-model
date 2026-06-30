#include "sep_virt_console.h"

#include <string>

SimVirtConsole::SimVirtConsole(sc_core::sc_module_name name)
    : sc_core::sc_module(name),
      verbosity("verbosity", CSML_DEFAULT_VERBOSITY),
      enable("enable", true) {

    logger.setMaxVerbosity(verbosity.get_param_value());
    // SIM_OUT is a *literal* source field rather than %MODULE% (which resolves to the
    // SystemC instance hierarchy and cannot be forced to a fixed token). This keeps
    // firmware lines visually consistent with internal VP status and reliably greppable.
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [SIM_OUT] - %MESSAGE%");
    logger.setFunctionTrace(false);

    decoder_.set_enabled(enable.get_param_value());
    decoder_.set_emit([this](const std::string& line) {
        // Emit at verbosity level 2 so SIM_OUT shows at the platform default verbosity.
        // LogStream appends the newline; do not add std::endl here.
        CSML_INFO(2, logger) << line;
    });
}

SimVirtConsole::~SimVirtConsole() {
    // Flush any buffered, unterminated content at end of simulation.
    decoder_.flush();
}
