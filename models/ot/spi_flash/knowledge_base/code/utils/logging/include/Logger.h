#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <ctime>

namespace utils {
namespace logging {

class Logger {
public:
    enum class LogLevel {
        TRACE = 0,
        DEBUG_LEVEL = 1,
        INFO = 2,
        WARNING = 3,
        ERROR = 4,
        FATAL = 5
    };

    struct LogEntry {
        time_t timestamp;
        LogLevel level;
        std::string function_name;
        int line_number;
        std::string message;
        std::string module_name;
    };

private:
    std::string module_name_;
    LogLevel min_level_;
    std::vector<std::ostream *> m_targets_;           // Keep existing approach
    std::vector<std::ofstream*> file_outputs_;  // Track file streams
    
    // Formatting options
    bool show_timestamp_;
    bool show_function_names_;
    bool show_line_numbers_;
    std::string timestamp_format_;

public:
    explicit Logger(const std::string& module_name);
    ~Logger();

    // Keep existing methods for backward compatibility
    void set_target(std::ostream &ref) {
        for (size_t i = 0; i < m_targets_.size(); ++i) {
            if (m_targets_[i] == &ref) {
                return; // avoid duplicate targets
            }
        }
        m_targets_.push_back(&ref);
    }
    void clear_targets() { m_targets_.clear(); }

    // Configuration
    void set_min_level(LogLevel level);
    
    // Convenience methods for common setups
    void setup_console_only() {
        clear_targets();
        set_target(std::cout);
    }
    
    void setup_file_only(const std::string& filename) {
        clear_targets();
        std::ofstream* file = new std::ofstream(filename.c_str(), std::ios::app);
        if (file->is_open()) {
            file_outputs_.push_back(file);
            set_target(*file);
        }
    }
    
    void setup_dual_output(const std::string& filename) {
        clear_targets();
        set_target(std::cout);  // Console
        std::ofstream* file = new std::ofstream(filename.c_str(), std::ios::app);
        if (file->is_open()) {
            file_outputs_.push_back(file);
            set_target(*file);  // File
        }
    }
    
    // Formatting options
    void set_show_timestamp(bool show);
    void set_show_function_names(bool show);
    void set_show_line_numbers(bool show);
    void set_timestamp_format(const std::string& format);

    // Main logging function with enhanced features
    void log(LogLevel level, const std::string& function_name, int line_number,
             const std::string& message);

    // Convenience methods for common configurations
    void setup_default_config();
    void setup_debug_config();
    void setup_production_config();

private:
    bool should_log(LogLevel level) const;
    std::string format_timestamp() const;
    std::string level_to_string(LogLevel level) const;
    std::string format_message(const LogEntry& entry) const;
    void write_to_targets(const std::string& formatted_message);
};

} // namespace logging
} // namespace utils
