# Software Design Document (SWDD)
## Project: Embedded AI Object Detection on Jetson Nano 4GB (Yocto Linux)

---

## 1. Package Structure & Modular Decomposition

The project source code is arranged cleanly into three directories matching the architectural tiers:

```
src/
├── app/                  # Tier 1: Application Layer
│   ├── main.cpp          # Process entry point, argument parsing
│   ├── edge_ai_manager   # Orchestrator & Dependency Injection
│   ├── pipeline          # Per-frame sequencing
│   └── rule_engine       # Domain detection rules
│
├── service/              # Tier 2: Service Layer
│   ├── camera_service    # Frame acquisition & ring buffering
│   ├── inference_service # YOLOv8 pipeline (pre/post-process)
│   ├── tracker_service   # ByteTrack tracking algorithm
│   ├── event_bus         # In-process asynchronous Pub/Sub
│   ├── mqtt_service      # Cloud telemetry & command handling
│   ├── storage_service   # SQLite ring buffer persistence
│   ├── health_service    # Watchdog & thermal monitoring
│   └── config_service    # YAML loader & validator
│
└── hal/                  # Tier 3: Hardware Abstraction Layer
    ├── jetson/           # Concrete Jetson Nano implementations
    │   ├── jetson_camera_hal.cpp    (GStreamer nvarguscamerasrc)
    │   ├── jetson_gpu_hal.cpp       (CUDA 10.2 / TensorRT 8.2)
    │   ├── jetson_network_hal.cpp   (Paho MQTT C library)
    │   ├── jetson_storage_hal.cpp   (SQLite3 C API)
    │   ├── jetson_sensor_hal.cpp    (Linux /sys & /proc files)
    │   └── jetson_watchdog_hal.cpp  (Linux /dev/watchdog ioctl)
    └── mock/             # Mock HALs for 100% Host PC Unit Testing
```

---

## 2. Dependency Injection & Object Lifecycle

Instead of using singletons or global state, all dependencies are instantiated in `EdgeAIManager::initialize()` and injected via constructor references or interfaces:

```cpp
// Lifecycle instantiation order in EdgeAIManager:
// 1. Create HAL instances (Jetson or Mock)
camera_hal_   = std::make_unique<hal::JetsonCameraHAL>();
gpu_hal_      = std::make_unique<hal::JetsonGpuHAL>();
network_hal_  = std::make_unique<hal::JetsonNetworkHAL>();
storage_hal_  = std::make_unique<hal::JetsonStorageHAL>();
sensor_hal_   = std::make_unique<hal::JetsonSensorHAL>();
watchdog_hal_ = std::make_unique<hal::JetsonWatchdogHAL>();

// 2. Inject HALs into Services
event_bus_      = std::make_unique<EventBus>(256);
camera_svc_     = std::make_unique<CameraService>(*camera_hal_);
inference_svc_  = std::make_unique<InferenceService>(*gpu_hal_);
tracker_svc_    = std::make_unique<TrackerService>(tracker_cfg);
mqtt_svc_       = std::make_unique<MqttService>(*network_hal_);
storage_svc_    = std::make_unique<StorageService>(*storage_hal_);
health_svc_     = std::make_unique<HealthService>(*sensor_hal_, *watchdog_hal_, *event_bus_);

// 3. Inject Services into App components
pipeline_    = std::make_unique<Pipeline>(*camera_svc_, *inference_svc_, *tracker_svc_);
rule_engine_ = std::make_unique<RuleEngine>(*event_bus_);
```

---

## 3. Class Design & Inheritance Hierarchy

### 3.1 HAL Interfaces
* `ICameraHAL`: pure virtual interface defining `open()`, `readFrame()`, `getFormat()`, `release()`.
* `IGpuHAL`: pure virtual interface defining `allocateBuffer()`, `copyHostToDevice()`, `runInference()`, `loadEngine()`.
* `INetworkHAL`: pure virtual interface defining `connect()`, `disconnect()`, `publish()`, `subscribe()`.
* `IStorageHAL`: pure virtual interface defining `open()`, `execute()`, `query()`, `beginTransaction()`.
* `ISensorHAL`: pure virtual interface defining `readTemperature()`, `readCpuUsage()`, `readGpuUsage()`.
* `IWatchdogHAL`: pure virtual interface defining `open()`, `kick()`, `close()`.

### 3.2 Rule Engine Polymorphism
* `IRule` (abstract base):
  * `virtual std::string getId() const = 0;`
  * `virtual std::optional<Alert> evaluate(const std::vector<TrackedObject>&) = 0;`
* Derived Concrete Rules:
  * `ZoneRule`: Detects if an object's bounding box center is inside an arbitrary polygon (Ray-Casting Algorithm).
  * `CountingRule`: Detects when an object's trajectory vector crosses a designated line segment.

---

## 4. Testability Strategy: Host PC Development with Mock HAL

One of the greatest advantages of the 3-layer architecture is host PC compilation:
* Developers on x86 Ubuntu / Windows can compile and run tests with:
  ```bash
  cmake -B build -DUSE_MOCK_HAL=ON
  cmake --build build
  ctest --test-dir build
  ```
* `MockGpuHAL` returns synthetic bounding boxes without needing an NVIDIA GPU or TensorRT.
* `MockCameraHAL` streams sample MP4/JPEG frames without needing a physical CSI camera.
* All Business Logic in `RuleEngine`, `TrackerService`, `EventBus`, and `Pipeline` is tested thoroughly in continuous integration (CI).

