#include "jetson_sensor_hal.h"
#include <fstream>
#include <sstream>
#include <iostream>

#if !defined(_WIN32)
#include <sys/statvfs.h>
#endif

namespace edge_ai { namespace hal {

float JetsonSensorHAL::readThermalZone(int zone_index) {
    std::string path = "/sys/devices/virtual/thermal/thermal_zone" + std::to_string(zone_index) + "/temp";
    std::ifstream file(path);
    if (!file.is_open()) {
        return 45.0f; // Default nominal temperature if unreadable
    }
    int millicelsius = 0;
    file >> millicelsius;
    return static_cast<float>(millicelsius) / 1000.0f;
}

float JetsonSensorHAL::readCpuTemperature() {
    // Thermal zone 1 is CPU on Tegra X1
    return readThermalZone(1);
}

float JetsonSensorHAL::readGpuTemperature() {
    // Thermal zone 2 is GPU on Tegra X1
    return readThermalZone(2);
}

float JetsonSensorHAL::readCpuUsage() {
    std::ifstream file("/proc/stat");
    if (!file.is_open()) return 20.0f;

    std::string cpu;
    uint64_t user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0, steal = 0;
    file >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

    uint64_t current_idle = idle + iowait;
    uint64_t current_total = user + nice + system + current_idle + irq + softirq + steal;

    uint64_t delta_idle = current_idle - prev_idle_time_;
    uint64_t delta_total = current_total - prev_total_time_;

    prev_idle_time_ = current_idle;
    prev_total_time_ = current_total;

    if (delta_total == 0) return 0.0f;
    return (1.0f - static_cast<float>(delta_idle) / static_cast<float>(delta_total)) * 100.0f;
}

float JetsonSensorHAL::readGpuUsage() {
    std::ifstream file("/sys/devices/gpu.0/load");
    if (!file.is_open()) return 50.0f;

    int load = 0;
    file >> load;
    return static_cast<float>(load) / 10.0f; // Often reported in tenths of a percent
}

MemInfo JetsonSensorHAL::readMemoryInfo() {
    MemInfo info;
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) {
        info.total_mb = 3964;
        info.available_mb = 2000;
        info.used_mb = 1964;
        return info;
    }

    std::string line;
    uint64_t total_kb = 0, avail_kb = 0;
    while (std::getline(file, line)) {
        if (line.rfind("MemTotal:", 0) == 0) {
            std::istringstream ss(line.substr(9));
            ss >> total_kb;
        } else if (line.rfind("MemAvailable:", 0) == 0) {
            std::istringstream ss(line.substr(13));
            ss >> avail_kb;
        }
    }

    info.total_mb = total_kb / 1024;
    info.available_mb = avail_kb / 1024;
    info.used_mb = (total_kb > avail_kb) ? (total_kb - avail_kb) / 1024 : 0;
    return info;
}

DiskInfo JetsonSensorHAL::readDiskInfo(const std::string& mount_path) {
    DiskInfo info;
#if !defined(_WIN32)
    struct statvfs stat;
    if (statvfs(mount_path.c_str(), &stat) == 0) {
        uint64_t total = (stat.f_blocks * stat.f_frsize) / (1024 * 1024);
        uint64_t avail = (stat.f_bavail * stat.f_frsize) / (1024 * 1024);
        info.total_mb = total;
        info.available_mb = avail;
        info.used_mb = (total > avail) ? total - avail : 0;
        return info;
    }
#else
    (void)mount_path;
#endif
    info.total_mb = 64000;
    info.available_mb = 45000;
    info.used_mb = 19000;
    return info;
}

}} // namespace edge_ai::hal

