#pragma once

#include <cstdint>
#include <string>

namespace edge_ai { namespace hal {

struct MemInfo {
    uint64_t total_mb{0};
    uint64_t available_mb{0};
    uint64_t used_mb{0};
};

struct DiskInfo {
    uint64_t total_mb{0};
    uint64_t available_mb{0};
    uint64_t used_mb{0};
};

class ISensorHAL {
public:
    virtual ~ISensorHAL() = default;

    virtual float readCpuTemperature() = 0; // Celsius
    virtual float readGpuTemperature() = 0; // Celsius
    virtual float readCpuUsage() = 0;       // Percentage (0.0 to 100.0)
    virtual float readGpuUsage() = 0;       // Percentage (0.0 to 100.0)
    virtual MemInfo readMemoryInfo() = 0;
    virtual DiskInfo readDiskInfo(const std::string& mount_path = "/") = 0;
};

}} // namespace edge_ai::hal

