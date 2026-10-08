#pragma once

#include "hal/i_sensor_hal.h"

namespace edge_ai { namespace hal {

class MockSensorHAL : public ISensorHAL {
public:
    MockSensorHAL() = default;
    ~MockSensorHAL() override = default;

    float readCpuTemperature() override { return 48.5f; }
    float readGpuTemperature() override { return 52.0f; }
    float readCpuUsage() override { return 35.2f; }
    float readGpuUsage() override { return 71.0f; }

    MemInfo readMemoryInfo() override {
        MemInfo m;
        m.total_mb = 3964;
        m.available_mb = 2100;
        m.used_mb = 1864;
        return m;
    }

    DiskInfo readDiskInfo(const std::string& /*mount_path*/ = "/") override {
        DiskInfo d;
        d.total_mb = 59000;
        d.available_mb = 45000;
        d.used_mb = 14000;
        return d;
    }
};

}} // namespace edge_ai::hal

