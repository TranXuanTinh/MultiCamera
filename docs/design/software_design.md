# Software Design Document (SWDD)
## Project: Multi-Camera Edge AI Video Analytics on Jetson with NVIDIA DeepStream SDK

---

## 1. Package Structure & Modular Decomposition

The project source code is arranged into 3 clean architectural tiers:

```
src/
├── app/                  # Tier 1: Application Layer
│   ├── main.cpp          # CLI entry point, argument parsing
│   ├── edge_ai_manager   # Orchestrator & Dependency Injection
│   ├── pipeline          # DeepStream pipeline coordinator & frame stepping
│   └── rule_engine       # Multi-Camera domain detection rules
│
├── service/              # Tier 2: Service Layer
│   ├── event_bus         # In-process asynchronous Pub/Sub
│   ├── mqtt_service      # Multi-camera telemetry & alert publishing
│   ├── storage_service   # SQLite ring buffer persistence (camera_id indexed)
│   ├── health_service    # Watchdog & thermal monitoring
│   └── config_service    # YAML loader & DeepStream config validator
│
└── hal/                  # Tier 3: Hardware Abstraction Layer
    ├── jetson/           # Concrete Jetson DeepStream implementations
    │   ├── jetson_deepstream_hal.cpp (nvstreammux, nvinfer, nvtracker, pad probe)
    │   ├── jetson_camera_hal.cpp     (Legacy GStreamer nvarguscamerasrc)
    │   ├── jetson_gpu_hal.cpp        (CUDA / TensorRT runtime)
    │   ├── jetson_network_hal.cpp    (Paho MQTT C library)
    │   ├── jetson_storage_hal.cpp    (SQLite3 C API)
    │   ├── jetson_sensor_hal.cpp     (Linux /sys thermal & load sensors)
    │   └── jetson_watchdog_hal.cpp   (Linux /dev/watchdog ioctl)
    └── mock/             # Mock HALs for 100% Host PC Unit Testing
        ├── mock_deepstream_hal.h     (Concurrent multi-camera simulation)
        ├── mock_camera_hal.h         (Synthetic frame generation)
        └── mock_gpu_hal.h            (Synthetic bounding box generation)
```

---

## 2. Dependency Injection & Object Lifecycle

All dependencies are instantiated in `EdgeAIManager::initialize()` and injected via constructor interfaces, avoiding any global singletons:

```cpp
// 1. Instantiate HALs (Jetson Production vs. Mock Host Mode)
if (use_mock_hal) {
    deepstream_hal_ = std::make_unique<hal::MockDeepStreamHAL>();
    network_hal_    = std::make_unique<hal::MockNetworkHAL>();
    storage_hal_    = std::make_unique<hal::MockStorageHAL>();
    sensor_hal_     = std::make_unique<hal::MockSensorHAL>();
    watchdog_hal_   = std::make_unique<hal::MockWatchdogHAL>();
} else {
    deepstream_hal_ = std::make_unique<hal::JetsonDeepStreamHAL>();
    network_hal_    = std::make_unique<hal::JetsonNetworkHAL>();
    storage_hal_    = std::make_unique<hal::JetsonStorageHAL>();
    sensor_hal_     = std::make_unique<hal::JetsonSensorHAL>();
    watchdog_hal_   = std::make_unique<hal::JetsonWatchdogHAL>();
}

// 2. Initialize DeepStream Pipeline HAL with multi-camera configuration
deepstream_hal_->initialize(config_->getDeepStreamConfig(), config_->getCameras());

// 3. Instantiate Platform-Independent Services
event_bus_   = std::make_unique<EventBus>(256);
mqtt_svc_    = std::make_unique<MqttService>(*network_hal_);
storage_svc_ = std::make_unique<StorageService>(*storage_hal_);
health_svc_  = std::make_unique<HealthService>(*sensor_hal_, *watchdog_hal_, *event_bus_);

// 4. Instantiate App Layer & Connect Pad Probe Callback
pipeline_    = std::make_unique<Pipeline>(*deepstream_hal_);
rule_engine_ = std::make_unique<RuleEngine>(*event_bus_);

pipeline_->setResultCallback([this](const PipelineResult& result) {
    onPipelineResult(result);
});
```

---

## 3. Class Design & Inheritance Hierarchy

### 3.1 DeepStream Hardware Abstraction Layer (`IDeepStreamHAL`)
* Pure virtual contract:
  * `virtual bool initialize(const DeepStreamConfig&, const std::vector<CameraSourceConfig>&) = 0;`
  * `virtual bool start() = 0;`
  * `virtual void stop() = 0;`
  * `virtual void setMetadataCallback(MetadataCallback cb) = 0;`
  * `virtual bool stepFrame(PipelineResult& out_result) = 0;`
* **Implementations**:
  * `JetsonDeepStreamHAL`: Implements full GStreamer pipeline with `nvstreammux`, `nvinfer` PGIE, `nvtracker`, `nvmultistreamtiler`, `nvdsosd`, and pad probe callback.
  * `MockDeepStreamHAL`: Implements multi-stream synthetic frame and detection simulation for host development.

### 3.2 Multi-Camera Rule Engine Polymorphism
* `IRule` (abstract base):
  * `virtual std::string getId() const = 0;`
  * `virtual std::string getName() const = 0;`
  * `virtual std::string getCameraId() const = 0;` // Specific camera (e.g. "cam_01") or wildcard ("*")
  * `virtual std::optional<Alert> evaluate(const std::vector<TrackedObject>&) = 0;`
* **Derived Concrete Rules**:
  * `ZoneRule`: Detects when an object belonging to a designated camera enters a polygon area (Ray-Casting Algorithm).
  * `CountingRule`: Detects when an object belonging to a designated camera crosses a line segment, maintaining per-camera counts.

---

## 4. Testability Strategy: Host PC Development with Mock HAL

* Developers can build and test the full multi-camera application on x86 Ubuntu / Windows without NVIDIA Jetson hardware:
  ```bash
  mkdir build && cd build
  cmake .. -DBUILD_TESTS=ON
  cmake --build .
  ctest --output-on-failure
  ```
* `MockDeepStreamHAL` generates realistic multi-channel detection streams for multiple virtual cameras, testing pad probe extraction, per-camera rule evaluation, MQTT serialization, and SQLite persistence.
