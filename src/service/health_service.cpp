#include "service/health_service.h"
#include <chrono>

namespace edge_ai {

HealthService::HealthService(hal::ISensorHAL& sensor_hal,
                             hal::IWatchdogHAL& watchdog_hal,
                             EventBus& event_bus)
    : sensor_(sensor_hal), watchdog_(watchdog_hal), event_bus_(event_bus) {
}

HealthService::~HealthService() {
    stop();
}

bool HealthService::initialize(const HealthConfig& config) {
    config_ = config;

    if (config_.watchdog_enabled) {
        watchdog_.open(config_.watchdog_device, config_.watchdog_timeout_sec);
    }
    return true;
}

void HealthService::start() {
    if (running_) return;
    running_ = true;
    monitor_thread_ = std::thread(&HealthService::monitorLoop, this);
}

void HealthService::stop() {
    if (!running_) return;
    running_ = false;

    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
    watchdog_.close();
}

void HealthService::updateInferenceMetrics(float fps, float latency_ms) {
    current_fps_ = fps;
    current_latency_ms_ = latency_ms;
}

HealthStatus HealthService::getLatestStatus() const {
    std::lock_guard<std::mutex> lock(status_mtx_);
    return latest_status_;
}

void HealthService::monitorLoop() {
    while (running_) {
        // Kick hardware watchdog to prove process is alive and responsive
        if (config_.watchdog_enabled && watchdog_.isOpen()) {
            watchdog_.kick();
        }

        HealthStatus status;
        status.cpu_temp = sensor_.readCpuTemperature();
        status.gpu_temp = sensor_.readGpuTemperature();
        status.cpu_usage = sensor_.readCpuUsage();
        status.gpu_usage = sensor_.readGpuUsage();

        hal::MemInfo mem = sensor_.readMemoryInfo();
        status.ram_total_mb = mem.total_mb;
        status.ram_used_mb = mem.used_mb;

        hal::DiskInfo disk = sensor_.readDiskInfo("/");
        status.disk_usage_pct = (disk.total_mb > 0) ?
            (static_cast<float>(disk.used_mb) / disk.total_mb * 100.0f) : 0.0f;

        status.avg_fps = current_fps_;
        status.avg_latency_ms = current_latency_ms_;

        if (status.cpu_temp >= config_.cpu_temp_critical) {
            status.status = "critical";
        } else if (status.cpu_temp >= config_.cpu_temp_warn) {
            status.status = "warning";
        } else {
            status.status = "healthy";
        }

        {
            std::lock_guard<std::mutex> lock(status_mtx_);
            latest_status_ = status;
        }

        // Publish to EventBus
        Event evt;
        evt.type = EventType::HEALTH_STATUS;
        evt.timestamp = std::chrono::system_clock::now();
        evt.data = status;
        event_bus_.publish(std::move(evt));

        // Sleep for designated interval
        for (int i = 0; i < config_.interval_sec && running_; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

} // namespace edge_ai

