#include "app/edge_ai_manager.h"

// Mock HALs
#include "hal/mock/mock_camera_hal.h"
#include "hal/mock/mock_gpu_hal.h"
#include "hal/mock/mock_network_hal.h"
#include "hal/mock/mock_storage_hal.h"
#include "hal/mock/mock_sensor_hal.h"
#include "hal/mock/mock_watchdog_hal.h"

// Concrete Jetson HALs
#include "hal/jetson/jetson_camera_hal.h"
#include "hal/jetson/jetson_gpu_hal.h"
#include "hal/jetson/jetson_network_hal.h"
#include "hal/jetson/jetson_storage_hal.h"
#include "hal/jetson/jetson_sensor_hal.h"
#include "hal/jetson/jetson_watchdog_hal.h"

#include <iostream>
#include <chrono>
#include <thread>

namespace edge_ai {

std::atomic<bool> EdgeAIManager::stop_requested{false};

void EdgeAIManager::signalHandler(int signum) {
    (void)signum;
    stop_requested = true;
}

EdgeAIManager::EdgeAIManager() = default;

EdgeAIManager::~EdgeAIManager() {
    shutdown();
}

void EdgeAIManager::createHal(bool use_mock_hal) {
    if (use_mock_hal) {
        LogService::info("Instantiating MOCK Hardware Abstraction Layer (Host PC Test Mode)...");
        camera_hal_   = std::make_unique<hal::MockCameraHAL>();
        gpu_hal_      = std::make_unique<hal::MockGpuHAL>();
        network_hal_  = std::make_unique<hal::MockNetworkHAL>();
        storage_hal_  = std::make_unique<hal::MockStorageHAL>();
        sensor_hal_   = std::make_unique<hal::MockSensorHAL>();
        watchdog_hal_ = std::make_unique<hal::MockWatchdogHAL>();
    } else {
        LogService::info("Instantiating JETSON Hardware Abstraction Layer (Production Mode)...");
        camera_hal_   = std::make_unique<hal::JetsonCameraHAL>();
        gpu_hal_      = std::make_unique<hal::JetsonGpuHAL>();
        network_hal_  = std::make_unique<hal::JetsonNetworkHAL>();
        storage_hal_  = std::make_unique<hal::JetsonStorageHAL>();
        sensor_hal_   = std::make_unique<hal::JetsonSensorHAL>();
        watchdog_hal_ = std::make_unique<hal::JetsonWatchdogHAL>();
    }
}

void EdgeAIManager::createServices() {
    event_bus_     = std::make_unique<EventBus>(256);
    camera_svc_    = std::make_unique<CameraService>(*camera_hal_, 2);
    inference_svc_ = std::make_unique<InferenceService>(*gpu_hal_);
    tracker_svc_   = std::make_unique<TrackerService>(config_->getTrackerConfig());
    mqtt_svc_      = std::make_unique<MqttService>(*network_hal_);
    storage_svc_   = std::make_unique<StorageService>(*storage_hal_);
    health_svc_    = std::make_unique<HealthService>(*sensor_hal_, *watchdog_hal_, *event_bus_);

    pipeline_    = std::make_unique<Pipeline>(*camera_svc_, *inference_svc_, *tracker_svc_);
    rule_engine_ = std::make_unique<RuleEngine>(*event_bus_);
}

void EdgeAIManager::wireEventBus() {
    // 1. Subscribe MqttService & StorageService to TRACKED_OBJECTS events
    event_bus_->subscribe(EventType::TRACKED_OBJECTS, [this](const Event& evt) {
        if (!evt.data.has_value()) return;
        const auto& result = evt.as<PipelineResult>();

        mqtt_svc_->publishDetections(result, config_->getDeviceId());

        for (const auto& obj : result.objects) {
            storage_svc_->recordDetection(result.frame_id, obj);
        }
    });

    // 2. Subscribe MqttService & StorageService to ALERT events
    event_bus_->subscribe(EventType::ALERT, [this](const Event& evt) {
        if (!evt.data.has_value()) return;
        const auto& alert = evt.as<Alert>();

        LogService::warn("ALERT TRIGGERED: [" + alert.rule_name + "] " + alert.message);
        mqtt_svc_->publishAlert(alert, config_->getDeviceId());
        storage_svc_->recordAlert(alert);
    });

    // 3. Subscribe MqttService to HEALTH_STATUS events
    event_bus_->subscribe(EventType::HEALTH_STATUS, [this](const Event& evt) {
        if (!evt.data.has_value()) return;
        const auto& health = evt.as<HealthStatus>();

        mqtt_svc_->publishHealth(health, config_->getDeviceId());
    });
}

bool EdgeAIManager::initialize(const std::string& config_file, bool use_mock_hal) {
    LogService::init(LogLevel::Info);
    LogService::info("Starting Edge AI Object Detection System initialization...");

    // 1. Load configuration
    config_ = std::make_unique<ConfigService>();
    if (!config_->loadFromFile(config_file)) {
        LogService::warn("Could not load YAML config from " + config_file + ", using compiled defaults.");
    }

    // 2. Instantiate HAL
    createHal(use_mock_hal);

    // 3. Instantiate Services
    createServices();

    // 4. Wire EventBus
    wireEventBus();
    event_bus_->start();

    // 5. Initialize Storage & MQTT
    if (!storage_svc_->initialize(config_->getStorageConfig())) {
        LogService::warn("Failed to initialize SQLite storage service.");
    }

    if (!mqtt_svc_->connect(config_->getMqttConfig())) {
        LogService::warn("MQTT broker connection failed, offline queue active.");
    }

    // 6. Initialize & Start Health Service
    if (health_svc_->initialize(config_->getHealthConfig())) {
        health_svc_->start();
    }

    // 7. Load Rules
    rule_engine_->loadRulesFromConfig(config_->getRulesConfigFile());

    // 8. Load Inference Model (TensorRT Engine)
    LogService::info("Loading YOLOv8 Model & TensorRT Engine...");
    if (!inference_svc_->loadModel(config_->getModelConfig())) {
        LogService::error("Failed to load model engine: " + config_->getModelConfig().engine_path);
        return false;
    }
    LogService::info("TensorRT Engine loaded successfully.");

    // 9. Open Camera
    LogService::info("Opening Camera Stream...");
    if (!camera_svc_->open(config_->getCameraConfig())) {
        LogService::error("Failed to open camera: " + config_->getCameraConfig().device);
        return false;
    }
    LogService::info("Camera streaming started.");

    // 10. Start Pipeline
    pipeline_->start();

    initialized_ = true;
    LogService::info("Initialization complete. System is ready.");
    return true;
}

void EdgeAIManager::run() {
    if (!initialized_) return;

    LogService::info("Entering primary detection loop...");

    while (!stop_requested) {
        PipelineResult result;
        if (pipeline_->processOneFrame(result)) {
            // Evaluate rules on tracked objects
            result.alerts = rule_engine_->evaluate(result.objects);

            // Update HealthService metrics
            health_svc_->updateInferenceMetrics(result.fps, result.inference_ms);

            // Publish results to EventBus (triggers MQTT & Storage asynchronously)
            Event evt;
            evt.type = EventType::TRACKED_OBJECTS;
            evt.timestamp = std::chrono::system_clock::now();
            evt.data = result;
            event_bus_->publish(std::move(evt));
        } else {
            // Avoid burning 100% CPU if camera has no frame
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    LogService::info("Stop signal caught. Exiting primary detection loop.");
}

void EdgeAIManager::shutdown() {
    if (!initialized_) return;
    initialized_ = false;

    LogService::info("Shutting down Edge AI Object Detection System gracefully...");

    if (pipeline_) pipeline_->stop();
    if (camera_svc_) camera_svc_->close();
    if (health_svc_) health_svc_->stop();
    if (storage_svc_) storage_svc_->shutdown();
    if (mqtt_svc_) mqtt_svc_->disconnect();
    if (event_bus_) event_bus_->stop();

    LogService::info("All services terminated cleanly.");
}

} // namespace edge_ai

