#pragma once
#include "Logger.h"
// No hex/string macros; stream formatting should be done at call-sites/macros

namespace utils {
namespace logging {

class LoggerConfig {
public:
    // Predefined configurations for different use cases
    static void setup_module_logger(Logger& logger, const std::string& module_name);
    
    // Environment-based configuration
    static void configure_from_env(Logger& logger);
    
    // File-based configuration (for future implementation)
    static void configure_from_file(Logger& logger, const std::string& config_file);
};

} // namespace logging
} // namespace utils
