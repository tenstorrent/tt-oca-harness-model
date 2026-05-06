#include "../include/LoggerConfig.h"
#include <cstdlib>
#include <fstream>
#include <iostream>

namespace utils {
namespace logging {

void LoggerConfig::setup_module_logger(Logger& logger, const std::string& module_name) {
    logger.set_min_level(Logger::LogLevel::INFO);
    logger.setup_dual_output(module_name + ".log");
    logger.set_show_timestamp(true);
    logger.set_show_function_names(true);
    logger.set_show_line_numbers(true);
}

void LoggerConfig::configure_from_env(Logger& logger) {
    // Check for LOG_LEVEL environment variable
    const char* log_level_env = std::getenv("LOG_LEVEL");
    if (log_level_env) {
        std::string level_str = log_level_env;
        if (level_str == "TRACE") {
            logger.set_min_level(Logger::LogLevel::TRACE);
        } else if (level_str == "DBG") {
            logger.set_min_level(Logger::LogLevel::DEBUG_LEVEL);
        } else if (level_str == "INFO") {
            logger.set_min_level(Logger::LogLevel::INFO);
        } else if (level_str == "WARNING") {
            logger.set_min_level(Logger::LogLevel::WARNING);
        } else if (level_str == "ERROR") {
            logger.set_min_level(Logger::LogLevel::ERROR);
        } else if (level_str == "FATAL") {
            logger.set_min_level(Logger::LogLevel::FATAL);
        }
    }
    
    // Check for LOG_FILE environment variable
    const char* log_file_env = std::getenv("LOG_FILE");
    if (log_file_env) {
        logger.setup_dual_output(log_file_env);
    }
    
    // Check for LOG_CONSOLE environment variable
    const char* log_console_env = std::getenv("LOG_CONSOLE");
    if (log_console_env && std::string(log_console_env) == "false") {
        logger.clear_targets();
        // Only keep file outputs, remove console
        const char* log_file_env = std::getenv("LOG_FILE");
        if (log_file_env) {
            logger.setup_file_only(log_file_env);
        }
    }
}

void LoggerConfig::configure_from_file(Logger& logger, const std::string& config_file) {
    // Placeholder for future file-based configuration
    // This could parse JSON, INI, or other configuration formats
    std::ifstream file(config_file);
    if (file.is_open()) {
        // TODO: Implement configuration file parsing
        std::cout << "Configuration file parsing not yet implemented" << std::endl;
        // For now, just apply default configuration
        logger.setup_default_config();
        file.close();
    } else {
        std::cerr << "Could not open configuration file: " << config_file << std::endl;
    }
}

} // namespace logging
} // namespace utils
