#include "../include/Logger.h"
#include "../include/LoggerConfig.h"
#include <iostream>
#include <sstream>
#include <ctime>
#include <algorithm>
#include <cstring>

namespace utils {
namespace logging {

Logger::Logger(const std::string& module_name) 
    : module_name_(module_name)
    , min_level_(LogLevel::INFO)
    , show_timestamp_(true)
    , show_function_names_(true)
    , show_line_numbers_(true)
    , timestamp_format_("%Y-%m-%d %H:%M:%S")
{
    // Default to console output so logs appear out-of-the-box
    setup_console_only();
    
    // Automatically check environment variables for all loggers
    LoggerConfig::configure_from_env(*this);
}

Logger::~Logger() {
    // Close all file outputs
    for (size_t i = 0; i < file_outputs_.size(); ++i) {
        if (file_outputs_[i]) {
            file_outputs_[i]->close();
            delete file_outputs_[i];
        }
    }
    file_outputs_.clear();
}

void Logger::log(LogLevel level, const std::string& function_name, int line_number,
                 const std::string& message) {
    if (!should_log(level)) {
        return;
    }

    LogEntry entry;
    entry.timestamp = time(NULL);
    entry.level = level;
    entry.function_name = function_name;
    entry.line_number = line_number;
    entry.message = message;
    entry.module_name = module_name_;

    std::string formatted_message = format_message(entry);
    write_to_targets(formatted_message);
}

bool Logger::should_log(LogLevel level) const {
    // Only check log level
    return level >= min_level_;
}

std::string Logger::format_timestamp() const {
    time_t now = time(NULL);
    struct tm* timeinfo = localtime(&now);
    char buffer[100];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return std::string(buffer);
}

std::string Logger::level_to_string(LogLevel level) const {
    switch (level) {
        case LogLevel::TRACE:   return "TRACE";
        case LogLevel::DEBUG_LEVEL:   return "DBG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        case LogLevel::FATAL:   return "FATAL";
        default:               return "UNKNOWN";
    }
}

std::string Logger::format_message(const LogEntry& entry) const {
    std::stringstream ss;
    
    if (show_timestamp_) {
        ss << "[" << format_timestamp() << "] ";
    }
    
    ss << "[" << level_to_string(entry.level) << "] ";
    
    // No component field anymore
    
    if (show_function_names_ && show_line_numbers_) {
        ss << "[" << entry.function_name << ":" << entry.line_number << "] ";
    } else if (show_function_names_) {
        ss << "[" << entry.function_name << "] ";
    } else if (show_line_numbers_) {
        ss << "[L" << entry.line_number << "] ";
    }
    
    ss << entry.message;
    
    return ss.str();
}

void Logger::write_to_targets(const std::string& formatted_message) {
    for (auto target : m_targets_) {
        if (target) {
            *target << formatted_message << std::endl;
            target->flush();
        }
    }
}

// Configuration methods
void Logger::set_min_level(LogLevel level) {
    min_level_ = level;
}


void Logger::set_show_timestamp(bool show) {
    show_timestamp_ = show;
}

void Logger::set_show_function_names(bool show) {
    show_function_names_ = show;
}

void Logger::set_show_line_numbers(bool show) {
    show_line_numbers_ = show;
}

void Logger::set_timestamp_format(const std::string& format) {
    timestamp_format_ = format;
}

// Convenience configuration methods
void Logger::setup_default_config() {
    set_min_level(LogLevel::INFO);
    setup_console_only();
    set_show_timestamp(true);
    set_show_function_names(true);
    set_show_line_numbers(true);
}

void Logger::setup_debug_config() {
    set_min_level(LogLevel::DEBUG_LEVEL);
    setup_dual_output(module_name_ + "_debug.log");
    set_show_timestamp(true);
    set_show_function_names(true);
    set_show_line_numbers(true);
}

void Logger::setup_production_config() {
    set_min_level(LogLevel::WARNING);
    setup_file_only(module_name_ + "_production.log");
    set_show_timestamp(true);
    set_show_function_names(false);
    set_show_line_numbers(false);
}

} // namespace logging
} // namespace utils
