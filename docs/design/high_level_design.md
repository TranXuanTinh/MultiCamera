# High-Level Design Document (HLD)
## Project: Multi-Camera Edge AI Video Analytics on Jetson with NVIDIA DeepStream SDK

---

## 1. Module Overview & Responsibilities

The system is decomposed into 3 distinct architectural tiers (Application, Service, and Hardware Abstraction Layer):

| Module | Tier | Responsibility | Primary Interactions |
|---|---|---|---|
| **main.cpp** | App | Process bootstrapper, CLI argument parser (`--config`, `--mock`), POSIX signal trap (`SIGINT`, `SIGTERM`), and exit code controller | Instantiates `EdgeAIManager`; coordinates startup, main loop, and graceful exit |
| **EdgeAIManager** | App | System lifecycle, dependency injection, service instantiation, and graceful shutdown | Owns all Service and HAL instances; routes DeepStream metadata callbacks |
| **Pipeline** | App | Coordinates video analytics pipeline execution, supporting both DeepStream hardware mode and modular service mode | Interfaces with `IDeepStreamHAL` or `CameraService` + `InferenceService` + `TrackerService` |
| **RuleEngine** | App | Evaluates spatial business rules (`ZoneRule`, `CountingRule`) with per-camera channel isolation, compound keying `[camera_id, track_id]`, and camera state resets on reconnect | Receives tracked objects tagged with `camera_id`; publishes alerts to `EventBus` |
| **CameraService** | Service | Manages camera capture lifecycle, frame buffer retrieval, reconnect logic, and resolution setting | Wraps `ICameraHAL`; delivers frames in modular/fallback mode |
| **InferenceService** | Service | Coordinates deep learning model inference, input scaling, GPU execution, and tensor parsing | Wraps `IGpuHAL`, `Preprocessor`, and `Postprocessor` |
| **TrackerService** | Service | Multi-object tracking (ByteTrack algorithm) maintaining persistent object trajectories across frames | Employs `KalmanFilter` for motion prediction and `Hungarian` for bipartite matching |
| **Preprocessor** | Service | Hardware-friendly input preprocessing: letterbox aspect-ratio padding, BGR-to-RGB, normalization ($1/255$), and CHW layout | Called by `InferenceService` before GPU inference |
| **Postprocessor** | Service | Decodes raw YOLOv8 `(1, 84, 8400)` output tensors: score filtering, class argmax, coordinate unletterboxing, and NMS | Called by `InferenceService` after GPU inference |
| **EventBus** | Service | In-process, non-blocking asynchronous Pub/Sub event dispatcher with bounded ring buffer (256 items) | Decouples video pipeline from MQTT network and SQLite disk I/O |
| **MqttService** | Service | Asynchronous telemetry & alert publisher with per-camera topic routing, camera telemetry metrics, and offline queue | Subscribes to `EventBus`, publishes JSON payloads via `INetworkHAL` |
| **StorageService** | Service | Embedded database persistence with bounded RAM write queue (1000 records), multi-camera schema, and batched SQLite transactions | Subscribes to `EventBus`, writes detections and alerts via `IStorageHAL` |
| **HealthService** | Service | Periodic thermal monitoring, CPU/GPU load, per-camera runtime metrics (`CameraRuntimeMetrics`), and hardware watchdog keepalive | Interacts with `ISensorHAL` (sysfs) and `IWatchdogHAL` (`/dev/watchdog`) |
| **ConfigService** | Service | Parses and validates `app_config.yaml`, `rules.yaml`, and DeepStream property files | Injected into all services and HALs during system initialization |
| **LogService** | Service | Centralized thread-safe asynchronous structured logging (`spdlog`) | Sinks to stdout, rotating log files, and systemd journal |
| **IDeepStreamHAL** | HAL | Pure virtual interface abstracting multi-camera GStreamer/DeepStream pipeline | Implemented by `JetsonDeepStreamHAL` and `MockDeepStreamHAL` |
| **JetsonDeepStreamHAL** | HAL | Production hardware pipeline: `nvstreammux` batching, `nvinfer`, `nvtracker`, `nvmultistreamtiler`, pad probe | Interacts with NVIDIA driver, NVMM memory, and GStreamer plugins |
| **MockDeepStreamHAL** | HAL | Simulates concurrent multi-camera video streams and detections on Host PC | Used for 100% test coverage and CI without Jetson hardware |
| **ICameraHAL** | HAL | Physical camera capture abstraction (MIPI CSI-2 via Argus or USB V4L2) | Implemented by `JetsonCameraHAL` and `MockCameraHAL` |
| **IGpuHAL** | HAL | CUDA runtime and TensorRT execution context abstraction | Implemented by `JetsonGpuHAL` and `MockGpuHAL` |
| **INetworkHAL** | HAL | Network communication and MQTT transport abstraction | Implemented by `JetsonNetworkHAL` (Paho) and `MockNetworkHAL` |
| **IStorageHAL** | HAL | Local database storage abstraction (SQLite3) | Implemented by `JetsonStorageHAL` and `MockStorageHAL` |
| **ISensorHAL** | HAL | System hardware telemetry abstraction (SoC thermal zones, CPU/GPU load) | Implemented by `JetsonSensorHAL` and `MockSensorHAL` |
| **IWatchdogHAL** | HAL | Hardware watchdog timer abstraction (`/dev/watchdog`) | Implemented by `JetsonWatchdogHAL` and `MockWatchdogHAL` |

---

## 2. High-Level Subsystem Interactions

```mermaid
flowchart TD
    OS["OS / systemd / CLI User"] -->|"Launch binary (-c, -m)"| MAIN

    subgraph APP["App Layer (Business Logic & Orchestration)"]
        MAIN["main.cpp<br/>(CLI Parser, POSIX Signals, Bootstrapper)"]
        MGR["EdgeAIManager<br/>(DI Container & Lifecycle Manager)"]
        PIPE["Pipeline (Dual Mode: DeepStream / Modular)"]
        RULE["RuleEngine (Per-Camera Spatial Rules)"]
    end

    subgraph SERVICES["Service Layer (Platform-Independent Services)"]
        direction TB
        subgraph ANALYTICS_SVC["Video Analytics Services"]
            CAM_S["CameraService"]
            INF_S["InferenceService<br/>(Preprocessor + Postprocessor)"]
            TRK_S["TrackerService<br/>(ByteTrack: Kalman + Hungarian)"]
        end

        subgraph BACKEND_SVC["Backend & Infrastructure Services"]
            EBUS["EventBus (Async Pub/Sub)"]
            MQTT_S["MqttService (Multi-Camera Telemetry)"]
            STOR_S["StorageService (SQLite Multi-Camera DB)"]
            HLT_S["HealthService (Watchdog & Thermal)"]
            CFG_S["ConfigService (YAML & DeepStream Configs)"]
            LOG_S["LogService (spdlog Async Logger)"]
        end
    end

    subgraph HALS["HAL Layer (Hardware & Framework Interfaces)"]
        DS_H["IDeepStreamHAL<br/>(JetsonDeepStreamHAL / MockDeepStreamHAL)"]
        CAM_H["ICameraHAL (CSI / V4L2)"]
        GPU_H["IGpuHAL (CUDA / TensorRT)"]
        NET_H["INetworkHAL (Paho MQTT)"]
        STR_H["IStorageHAL (SQLite3)"]
        SNS_H["ISensorHAL (Sysfs Sensors)"]
        WDG_H["IWatchdogHAL (/dev/watchdog)"]
    end

    %% Process Bootstrapping
    MAIN -->|"1. Instantiate & Initialize"| MGR
    MAIN -->|"2. Run Detection Loop"| MGR
    MAIN -->|"3. Signal Trap & Shutdown"| MGR

    %% Lifecycle & Configuration
    MGR --> CFG_S
    MGR --> LOG_S
    MGR --> DS_H
    MGR --> PIPE
    MGR --> RULE

    %% Primary Multi-Camera DeepStream Flow (Production Mode)
    PIPE --> DS_H
    DS_H -.->|"Pad Probe Batch Metadata"| PIPE
    PIPE --> RULE
    PIPE --> EBUS
    RULE --> EBUS

    %% Modular Fallback Services Flow (Diagnostic / Single-Camera Mode)
    PIPE -.-> CAM_S
    CAM_S --> CAM_H
    PIPE -.-> INF_S
    INF_S --> GPU_H
    PIPE -.-> TRK_S

    %% Event Bus Async Fan-Out
    EBUS -.->|"Async Detections"| MQTT_S
    MQTT_S --> NET_H

    EBUS -.->|"Async Detections"| STOR_S
    STOR_S --> STR_H

    EBUS -.->|"Async Alerts"| MQTT_S
    EBUS -.->|"Async Alerts"| STOR_S

    %% Health & Watchdog Keepalive
    HLT_S --> SNS_H
    HLT_S --> WDG_H
    HLT_S -.->|"Async Health Telemetry"| EBUS
```

---

## 3. Communication Patterns

### 3.1 Process Bootstrapping & Signal Trapping (`main.cpp`)
1. **Command-Line Interface**: The binary entry point `main(argc, argv)` in `src/app/main.cpp` parses runtime flags (`--config <path>`, `--mock`, `--help`, `--version`).
2. **Signal Trapping**: Hooks POSIX signals `SIGINT` (Ctrl+C) and `SIGTERM` (systemd daemon termination) to static method `EdgeAIManager::signalHandler()`, writing to an atomic boolean flag (`stop_requested`) without blocking.
3. **RAII Lifecycle**: Instantiates `EdgeAIManager` on the stack, ensuring automatic resource cleanup upon scope exit.
4. **Error Handling & Exit Codes**: Coordinates `app.initialize()`, `app.run()`, and `app.shutdown()`, returning exit code `0` on clean completion or code `1` upon fatal initialization failure.

### 3.2 Primary Path: Asynchronous DeepStream Pad Probe Callback
1. DeepStream executes natively in hardware GStreamer streaming threads (zero-copy NVMM memory).
2. A buffer probe (`GST_PAD_PROBE_TYPE_BUFFER`) on the `nvtracker` source pad intercepts `NvDsBatchMeta`.
3. For each frame in the batch:
   * Maps `frame_meta->source_id` to the corresponding `camera_id` (e.g. `0` $\to$ `cam_01`, `1` $\to$ `cam_02`).
   * Extracts detections, labels, and tracking IDs into a `PipelineResult`.
   * Invokes `Pipeline::onDeepStreamMetadata()`, which evaluates rules per camera and dispatches events to `EventBus`.

### 3.3 Modular Fallback Path: Service-Level Pipeline
When running in diagnostic or single-camera mode without DeepStream:
1. `CameraService` grabs a `cv::Mat` frame from `ICameraHAL`.
2. `InferenceService` uses `Preprocessor` to letterbox and normalize the frame, runs GPU inference via `IGpuHAL`, and parses bounding boxes via `Postprocessor`.
3. `TrackerService` updates Kalman trajectories and performs Hungarian association.
4. Results are pushed to `RuleEngine` and published to `EventBus`.

### 3.4 Non-Blocking Event Bus Distribution
* The probe thread or pipeline worker spends $< 0.05\text{ ms}$ pushing events into the thread-safe `EventBus` queue.
* Dedicated worker threads deliver events to `MqttService` and `StorageService` asynchronously.
* Neither network latency nor disk writes ever block frame processing or cause dropped video frames.

### 3.5 Periodic Hardware Monitoring & Watchdog
* `HealthService` executes every 30 seconds on a timer thread:
  1. Inspects SoC thermal zones and CPU/GPU utilization via `ISensorHAL`.
  2. Measures per-camera performance metrics (`CameraRuntimeMetrics`: `input_fps`, `processed_fps`, `dropped_frames`, `last_pts`, `decode_latency_ms`).
  3. Kicks Linux kernel `/dev/watchdog` via `IWatchdogHAL` (`ioctl(fd, WDIOC_KEEPALIVE, 0)`).
  4. Publishes enriched `HealthStatus` events to `EventBus` for multi-channel cloud telemetry.

### 3.6 Safe Ordered Shutdown Lifecycle
The system enforces an ordered 4-phase shutdown protocol:
1. **Stop Video Ingress**: Stop GStreamer streaming and DeepStream HAL first, preventing new frames from entering the pipeline.
2. **Drain Bounded Event Queue**: `EventBus::stop()` drains remaining in-flight events completely before stopping dispatch.
3. **Commit Persistent State & Network Teardown**: `StorageService::shutdown()` commits batched SQLite transactions; `MqttService::disconnect()` disengages network sockets.
4. **Hardware Watchdog Safe Release**: `HealthService::stop()` writes `'V'` to `/dev/watchdog` before closing, preventing emergency reboots during application exit.

---

## 4. Process Lifecycle & Bootstrapping Sequence

The following sequence details how the process transitions from operating system launch (via CLI or systemd) through initialization, runtime processing, signal interception, and final teardown:

```mermaid
sequenceDiagram
    autonumber
    actor OS as OS / systemd / User
    participant MAIN as main() Entry Point
    participant MGR as EdgeAIManager
    participant HAL as DeepStream HAL
    participant BUS as EventBus
    participant PIPE as Multi-Camera Pipeline

    OS->>MAIN: Execute binary (argv flags: -c, -m)
    activate MAIN
    MAIN->>MAIN: Parse CLI options & Install POSIX signal handlers
    MAIN->>MGR: Instantiate app instance on stack
    MAIN->>MGR: app.initialize(config_path, use_mock_hal)
    activate MGR
    MGR->>HAL: Initialize DeepStream HAL (nvstreammux batch=2)
    MGR->>BUS: Wire subscribers & start EventBus worker
    MGR->>PIPE: pipeline_->start()
    MGR-->>MAIN: Return true (Initialized)
    deactivate MGR

    MAIN->>MGR: app.run()
    activate MGR
    Note over MGR,PIPE: Main thread sleeps 50ms per iteration.<br/>GStreamer & CUDA execute on hardware threads.

    OS->>MAIN: Send SIGINT (Ctrl+C) or SIGTERM (systemctl stop)
    MAIN->>MGR: signalHandler() sets stop_requested = true
    Note over MGR: Next loop check detects stop_requested == true
    MGR-->>MAIN: Exit run() loop
    deactivate MGR

    MAIN->>MGR: app.shutdown()
    activate MGR
    Note over MGR: Executes 4-phase safe shutdown sequence
    MGR-->>MAIN: All services cleanly terminated
    deactivate MGR

    MAIN-->>OS: Return exit code 0
    deactivate MAIN
```


