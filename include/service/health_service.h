#pragma once

#include "app/types.h"
#include "hal/i_sensor_hal.h"
#include "hal/i_watchdog_hal.h"
#include "service/event_bus.h"
#include <thread>
#include <atomic>

namespace edge_ai {

class HealthService {
public:
    HealthService(hal::ISensorHAL& sensor_hal,
                  hal::IWatchdogHAL& watchdog_hal,
                  EventBus& event_bus);
    ~HealthService();

    bool initialize(const HealthConfig& config);
    void start();
    void stop();

    void updateInferenceMetrics(float fps, float latency_ms);
    HealthStatus getLatestStatus() const;

private:
    void monitorLoop();

    hal::ISensorHAL& sensor_;
    hal::IWatchdogHAL& watchdog_;
    EventBus& event_bus_;
    HealthConfig config_;

    std::atomic<bool> running_{false};
    std::thread monitor_thread_;

    std::atomic<float> current_fps_{0.0f};
    std::atomic<float> current_latency_ms_{0.0f};
    HealthStatus latest_status_;
    mutable std::mutex status_mtx_;
};

} // namespace edge_ai

