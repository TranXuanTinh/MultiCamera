#include "service/log_service.h"
#include <fstream>
#include <sstream>

namespace edge_ai {

LogLevel LogService::min_level_ = LogLevel::Info;
std::mutex LogService::log_mtx_;

void LogService::init(LogLevel min_level, const std::string& /*log_file*/) {
    min_level_ = min_level;
}

void LogService::log(LogLevel level, const std::string& msg) {
    if (level < min_level_) return;

    std::lock_guard<std::mutex> lock(log_mtx_);

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::ostringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count() << " ";

    switch (level) {
        case LogLevel::Trace: ss << "[TRACE] "; break;
        case LogLevel::Debug: ss << "[DEBUG] "; break;
        case LogLevel::Info:  ss << "[INFO ] "; break;
        case LogLevel::Warn:  ss << "[WARN ] "; break;
        case LogLevel::Error: ss << "[ERROR] "; break;
    }

    ss << msg << std::endl;
    std::cout << ss.str() << std::flush;
}

} // namespace edge_ai

