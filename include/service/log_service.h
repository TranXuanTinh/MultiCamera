#pragma once

#include <string>
#include <iostream>
#include <mutex>
#include <chrono>
#include <iomanip>

namespace edge_ai {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error
};

class LogService {
public:
    static void init(LogLevel min_level = LogLevel::Info, const std::string& log_file = "");

    static void log(LogLevel level, const std::string& msg);
    static void info(const std::string& msg)  { log(LogLevel::Info, msg); }
    static void warn(const std::string& msg)  { log(LogLevel::Warn, msg); }
    static void error(const std::string& msg) { log(LogLevel::Error, msg); }
    static void debug(const std::string& msg) { log(LogLevel::Debug, msg); }

private:
    static LogLevel min_level_;
    static std::mutex log_mtx_;
};

} // namespace edge_ai

