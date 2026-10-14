#include "app/edge_ai_manager.h"

// Mock HALs
#include "hal/mock/mock_deepstream_hal.h"
#include "hal/mock/mock_camera_hal.h"
#include "hal/mock/mock_gpu_hal.h"
#include "hal/mock/mock_network_hal.h"
#include "hal/mock/mock_storage_hal.h"
#include "hal/mock/mock_sensor_hal.h"
#include "hal/mock/mock_watchdog_hal.h"

// Concrete Jetson HALs
#include "hal/jetson/jetson_deepstream_hal.h"
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
        LogService::info("Instantiating MOCK DeepStream & Hardware Layer (Multi-Camera Host Simulation Mode)...");
        deepstream_hal_ = std::make_unique<hal::MockDeepStreamHAL>();
        camera_hal_     = std::make_unique<hal::MockCameraHAL>();
        gpu_hal_        = std::make_unique<hal::MockGpuHAL>();
        network_hal_    = std::make_unique<hal::MockNetworkHAL>();
        storage_hal_    = std::make_unique<hal::MockStorageHAL>();
        sensor_hal_     = std::make_unique<hal::MockSensorHAL>();
        watchdog_hal_   = std::make_unique<hal::MockWatchdogHAL>();
    } else {
        LogService::info("Instantiating JETSON DeepStream & Hardware Layer (Production Mode)...");
        deepstream_hal_ = std::make_unique<hal::JetsonDeepStreamHAL>();
        camera_hal_     = std::make_unique<hal::JetsonCameraHAL>();
        gpu_hal_        = std::make_unique<hal::JetsonGpuHAL>();
        network_hal_    = std::make_unique<hal::JetsonNetworkHAL>();
        storage_hal_    = std::make_unique<hal::JetsonStorageHAL>();
        sensor_hal_     = std::make_unique<hal::JetsonSensorHAL>();
        watchdog_hal_   = std::make_unique<hal::JetsonWatchdogHAL>();
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

    pipeline_    = std::make_unique<Pipeline>(*deepstream_hal_);
    rule_engine_ = std::make_unique<RuleEngine>(*event_bus_);

    pipeline_->setResultCallback([this](const PipelineResult& result) {
        onPipelineResult(result);
    });
}

void EdgeAIManager::wireEventBus() {
    // 1. Subscribe MqttService & StorageService to TRACKED_OBJECTS events
    event_bus_->subscribe(EventType::TRACKED_OBJECTS, [this](const Event& evt) {
        if (!evt.data.has_value()) return;
        const PipelineResult& result = evt.as<PipelineResult>();

        mqtt_svc_->publishDetections(result, config_->getDeviceId());

        for (const TrackedObject& obj : result.objects) {
            storage_svc_->recordDetection(result.camera_id, result.frame_id, obj);
        }
    });

    // 2. Subscribe MqttService & StorageService to ALERT events
    event_bus_->subscribe(EventType::ALERT, [this](const Event& evt) {
        if (!evt.data.has_value()) return;
        const Alert& alert = evt.as<Alert>();

        LogService::warn("ALERT TRIGGERED [" + alert.camera_id + " - " + alert.rule_name + "]: " + alert.message);
        mqtt_svc_->publishAlert(alert, config_->getDeviceId());
        storage_svc_->recordAlert(alert);
    });

    // 3. Subscribe MqttService to HEALTH_STATUS events
    event_bus_->subscribe(EventType::HEALTH_STATUS, [this](const Event& evt) {
        if (!evt.data.has_value()) return;
        const HealthStatus& health = evt.as<HealthStatus>();

        mqtt_svc_->publishHealth(health, config_->getDeviceId());
    });
}

void EdgeAIManager::onPipelineResult(const PipelineResult& result) {
    PipelineResult enriched = result;

    // Evaluate rules on tracked objects for this specific camera
    enriched.alerts = rule_engine_->evaluate(enriched.objects);

    // Update HealthService metrics
    health_svc_->updateInferenceMetrics(enriched.fps, enriched.inference_ms);

    // Publish results to EventBus (triggers MQTT & Storage asynchronously)
    Event evt;
    evt.type = EventType::TRACKED_OBJECTS;
    evt.timestamp = std::chrono::system_clock::now();
    evt.data = enriched;
    event_bus_->publish(std::move(evt));
}

bool EdgeAIManager::initialize(const std::string& config_file, bool use_mock_hal) {
    LogService::init(LogLevel::Info);
    LogService::info("Starting Multi-Camera DeepStream Object Detection System initialization...");

    // 1. Load configuration
    config_ = std::make_unique<ConfigService>();
    if (!config_->loadFromFile(config_file)) {
        LogService::warn("Could not load YAML config from " + config_file + ", using compiled defaults.");
    }

    // 2. Instantiate HAL
    createHal(use_mock_hal);

    // 3. Initialize DeepStream Pipeline HAL
    LogService::info("Initializing DeepStream Multi-Camera Pipeline with " +
                     std::to_string(config_->getCameras().size()) + " cameras...");
    if (!deepstream_hal_->initialize(config_->getDeepStreamConfig(), config_->getCameras())) {
        LogService::error("Failed to initialize DeepStream HAL pipeline");
        return false;
    }

    // 4. Instantiate Services
    createServices();

    // 5. Wire EventBus
    wireEventBus();
    event_bus_->start();

    // 6. Initialize Storage & MQTT
    if (!storage_svc_->initialize(config_->getStorageConfig())) {
        LogService::warn("Failed to initialize SQLite storage service.");
    }

    if (!mqtt_svc_->connect(config_->getMqttConfig())) {
        LogService::warn("MQTT broker connection failed, offline queue active.");
    }

    // 7. Initialize & Start Health Service
    if (health_svc_->initialize(config_->getHealthConfig())) {
        health_svc_->start();
    }

    // 8. Load Rules
    rule_engine_->loadRulesFromConfig(config_->getRulesConfigFile());

    // 9. Start Pipeline
    if (!pipeline_->start()) {
        LogService::error("Failed to start Multi-Camera DeepStream Pipeline");
        return false;
    }

    initialized_ = true;
    LogService::info("Initialization complete. DeepStream Multi-Camera System is ready.");
    return true;
}

void EdgeAIManager::run() {
    if (!initialized_) return;

    LogService::info("Entering multi-camera detection loop...");

    while (!stop_requested) {
        if (!pipeline_->isDeepStream()) {
            PipelineResult result;
            if (pipeline_->processOneFrame(result)) {
                onPipelineResult(result);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        } else {
            // DeepStream executes asynchronously in GStreamer background threads
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    LogService::info("Stop signal caught. Exiting primary detection loop.");
}

void EdgeAIManager::shutdown() {
    if (!initialized_) return;
    initialized_ = false;

    LogService::info("Shutting down Multi-Camera DeepStream System gracefully...");

    // 1. Stop video streaming ingestion & DeepStream pipeline (stops new frame arrival)
    if (pipeline_) pipeline_->stop();
    if (deepstream_hal_) deepstream_hal_->stop();
    if (camera_svc_) camera_svc_->close();

    // 2. Drain and stop bounded event bus queue (ensures all in-flight metadata is delivered to workers)
    if (event_bus_) event_bus_->stop();

    // 3. Gracefully flush and shutdown worker persistence & network services
    if (storage_svc_) storage_svc_->shutdown();
    if (mqtt_svc_) mqtt_svc_->disconnect();

    // 4. Stop health monitoring and safely close hardware watchdog
    if (health_svc_) health_svc_->stop();

    LogService::info("All services terminated cleanly in safe order.");
}

} // namespace edge_ai
