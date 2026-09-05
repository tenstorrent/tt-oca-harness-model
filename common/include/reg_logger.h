// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file reg_logger.h
 * @brief Stream logger used by SEP (and any other) models that previously
 *        used the external CSML logger. Header-only; no separate library.
 *
 * Prefer SIM_LOG_* from sim_log.h for new SMC-style models. This logger exists
 * so existing SEP testbenches that stream into a per-instance logger keep
 * working after the CSML submodule is removed.
 */

#pragma once

#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>

#include <systemc>

#ifndef REG_DEFAULT_VERBOSITY
#define REG_DEFAULT_VERBOSITY 2
#endif

enum reg_severity {
    REG_SEV_ERROR = 0,
    REG_SEV_WARN  = 1,
    REG_SEV_INFO  = 2,
    REG_SEV_DEBUG = 3
};

class RegLogger {
public:
    RegLogger()
        : maxVerbosity(REG_SEV_DEBUG),
          logFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%"),
          enableFunctionTrace(true),
          enableVariableTrace(true) {}

    void setMaxVerbosity(int level) { maxVerbosity = level; }
    int getMaxVerbosity() const { return maxVerbosity; }
    bool getFunctionTrace() const { return enableFunctionTrace; }
    bool getVariableTrace() const { return enableVariableTrace; }

    void setLogFormat(const std::string& format) { logFormat = format; }
    std::string getLogFormat() const { return logFormat; }
    void setFunctionTrace(bool value) { enableFunctionTrace = value; }
    void setVariableTrace(bool value) { enableVariableTrace = value; }

    static void setGlobalLogFile(const std::string& path) {
        globalLogFilePath_ = path;
        globalLogFileStream_ = std::make_unique<std::ofstream>(path, std::ios::out | std::ios::app);
        globalLogEnabled_ = globalLogFileStream_ && globalLogFileStream_->is_open();
    }
    static void disableGlobalLogFile() {
        globalLogEnabled_ = false;
        globalLogFileStream_.reset();
        globalLogFilePath_.clear();
    }
    static bool globalLogFileEnabled() {
        return globalLogEnabled_ && globalLogFileStream_ && globalLogFileStream_->is_open();
    }
    static std::ostream* globalLogFileStream() {
        if (!globalLogFileEnabled()) return nullptr;
        return globalLogFileStream_.get();
    }

private:
    int maxVerbosity;
    std::string logFormat;
    bool enableFunctionTrace;
    bool enableVariableTrace;

    inline static std::unique_ptr<std::ofstream> globalLogFileStream_;
    inline static std::string globalLogFilePath_;
    inline static bool globalLogEnabled_ = false;
};

class RegLogStream : public std::ostringstream {
public:
    RegLogStream(const std::map<std::string, std::string>& tokens, std::string format)
        : tokens_(tokens), format_(std::move(format)) {}
    ~RegLogStream() override {
        auto t = tokens_;
        t["%MESSAGE%"] = this->str();
        std::string result = format_;
        for (const auto& [token, value] : t) {
            size_t pos = 0;
            while ((pos = result.find(token, pos)) != std::string::npos) {
                result.replace(pos, token.size(), value);
                pos += value.size();
            }
        }
        std::cout << result << std::endl;
        if (auto* out = RegLogger::globalLogFileStream()) {
            (*out) << result << std::endl;
            out->flush();
        }
    }

private:
    std::map<std::string, std::string> tokens_;
    std::string format_;
};

inline std::string reg_logger_module_name() {
    auto* obj = sc_core::sc_get_current_object();
    if (!obj) return "";
    const char* n = obj->name();
    return n ? n : "";
}

inline RegLogStream regLog(const std::string& severity, int verbosity,
                           const char* func, RegLogger& logger) {
    std::map<std::string, std::string> tokens = {
        {"%MODULE%",   reg_logger_module_name()},
        {"%LEVEL%",    severity},
        {"%VERBOSITY%", std::to_string(verbosity)},
        {"%TIME%",     sc_core::sc_time_stamp().to_string()},
        {"%FUNCTION%", (func && *func) ? std::string(func) : std::string("unknown")},
        {"%MESSAGE%",  ""}
    };
    return RegLogStream(tokens, logger.getLogFormat());
}

#define REG_LOG(sev, level, logger) \
    if ((level) > (logger).getMaxVerbosity()) {} \
    else regLog(sev, level, __FUNCTION__, logger)

#define REG_DEBUG(level, logger) REG_LOG("DEBUG", level, logger)
#define REG_INFO(level, logger)  REG_LOG("INFO", level, logger)
#define REG_WARN(level, logger)  regLog("WARN", level, __FUNCTION__, logger)
#define REG_ERROR(level, logger) regLog("ERROR", level, __FUNCTION__, logger)

class RegFunctionTracer {
public:
    RegFunctionTracer(const char* func, RegLogger& logger)
        : funcName(func), logger(logger) {
        if (logger.getFunctionTrace())
            regLog("trace", REG_SEV_DEBUG, funcName, logger) << "Entered " << funcName;
    }
    ~RegFunctionTracer() {
        if (logger.getFunctionTrace())
            regLog("trace", REG_SEV_DEBUG, funcName, logger) << "Exiting " << funcName;
    }
private:
    const char* funcName;
    RegLogger& logger;
};

#define REG_FUNC_TRACE(logger) RegFunctionTracer __regFuncTracerObj(__FUNCTION__, logger)

template <typename T>
class RegVariableTracer {
public:
    RegVariableTracer(RegLogger& logger, const char* name, const T& value)
        : m_logger(logger), m_var_name(name), m_value(value) {}
    RegVariableTracer(RegLogger& logger, const char* name)
        : m_logger(logger), m_var_name(name) {}

    RegVariableTracer& operator=(const T& v) {
        if (m_logger.getVariableTrace()) {
            regLog("trace", REG_SEV_DEBUG, "", m_logger)
                << m_var_name << ": " << m_value << " -> " << v;
        }
        m_value = v;
        return *this;
    }
    RegVariableTracer& operator=(const RegVariableTracer& other) {
        return operator=(static_cast<T>(other));
    }
    operator T() const { return m_value; }
    RegVariableTracer& operator+=(const T& rhs) { return (*this = static_cast<T>(m_value + rhs)); }
    RegVariableTracer& operator-=(const T& rhs) { return (*this = static_cast<T>(m_value - rhs)); }
    RegVariableTracer& operator*=(const T& rhs) { return (*this = static_cast<T>(m_value * rhs)); }
    RegVariableTracer& operator/=(const T& rhs) { return (*this = static_cast<T>(m_value / rhs)); }
    RegVariableTracer& operator%=(const T& rhs) { return (*this = static_cast<T>(m_value % rhs)); }
    RegVariableTracer& operator&=(const T& rhs) { return (*this = static_cast<T>(m_value & rhs)); }
    RegVariableTracer& operator|=(const T& rhs) { return (*this = static_cast<T>(m_value | rhs)); }
    RegVariableTracer& operator^=(const T& rhs) { return (*this = static_cast<T>(m_value ^ rhs)); }
    RegVariableTracer& operator<<=(const T& rhs) { return (*this = static_cast<T>(m_value << rhs)); }
    RegVariableTracer& operator>>=(const T& rhs) { return (*this = static_cast<T>(m_value >> rhs)); }
    RegVariableTracer& operator++() { return (*this = static_cast<T>(m_value + 1)); }
    T operator++(int) { T tmp = m_value; *this = static_cast<T>(m_value + 1); return tmp; }
    RegVariableTracer& operator--() { return (*this = static_cast<T>(m_value - 1)); }
    T operator--(int) { T tmp = m_value; *this = static_cast<T>(m_value - 1); return tmp; }

private:
    RegLogger& m_logger;
    const char* m_var_name;
    T m_value;
};

#define REG_VAR_TRACE(type, var, logger, ...) \
    RegVariableTracer<type> var { logger, #var, ##__VA_ARGS__ }
