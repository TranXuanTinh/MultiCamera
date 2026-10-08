#pragma once

#include "hal/i_sensor_hal.h"
#include <string>

namespace edge_ai { namespace hal {

class JetsonSensorHAL : public ISensorHAL {
public:
    JetsonSensorHAL() = default;
    ~JetsonSensorHAL() override = default;

    float readCpuTemperature() override;
    float readGpuTemperature() override;
    float readCpuUsage() override;
    float readGpuUsage() override;
    MemInfo readMemoryInfo() override;
    DiskInfo readDiskInfo(const std::string& mount_path = "/") override;

private:
    float readThermalZone(int zone_index);
    uint64_t prev_idle_time_{0};
    uint64_t prev_total_time_{0};
};

}} // namespace edge_ai::hal

