#pragma once

#include "app/types.h"
#include "app/pipeline.h"
#include "app/rule_engine.h"

#include "service/config_service.h"
#include "service/log_service.h"
#include "service/event_bus.h"
#include "service/camera_service.h"
#include "service/inference_service.h"
#include "service/tracker_service.h"
#include "service/mqtt_service.h"
#include "service/storage_service.h"
#include "service/health_service.h"

#include "hal/i_camera_hal.h"
#include "hal/i_gpu_hal.h"
#include "hal/i_network_hal.h"
#include "hal/i_storage_hal.h"
#include "hal/i_sensor_hal.h"
#include "hal/i_watchdog_hal.h"
#include "hal/i_deepstream_hal.h"

#include <memory>
#include <atomic>
#include <string>

namespace edge_ai {

class EdgeAIManager {
public:
    EdgeAIManager();
    ~EdgeAIManager();

    bool initialize(const std::string& config_file, bool use_mock_hal = false);
    void run();
    void shutdown();

    static void signalHandler(int signum);
    static std::atomic<bool> stop_requested;

private:
    void createHal(bool use_mock_hal);
    void createServices();
    void wireEventBus();
    void onPipelineResult(const PipelineResult& result);

    // Configuration & Logging
    std::unique_ptr<ConfigService> config_;

    // HAL Instances
    std::unique_ptr<hal::IDeepStreamHAL> deepstream_hal_;
    std::unique_ptr<hal::ICameraHAL> camera_hal_;
    std::unique_ptr<hal::IGpuHAL> gpu_hal_;
    std::unique_ptr<hal::INetworkHAL> network_hal_;
    std::unique_ptr<hal::IStorageHAL> storage_hal_;
    std::unique_ptr<hal::ISensorHAL> sensor_hal_;
    std::unique_ptr<hal::IWatchdogHAL> watchdog_hal_;

    // Service Instances
    std::unique_ptr<EventBus> event_bus_;
    std::unique_ptr<CameraService> camera_svc_;
    std::unique_ptr<InferenceService> inference_svc_;
    std::unique_ptr<TrackerService> tracker_svc_;
    std::unique_ptr<MqttService> mqtt_svc_;
    std::unique_ptr<StorageService> storage_svc_;
    std::unique_ptr<HealthService> health_svc_;

    // App Components
    std::unique_ptr<Pipeline> pipeline_;
    std::unique_ptr<RuleEngine> rule_engine_;

    bool initialized_{false};
};

} // namespace edge_ai

