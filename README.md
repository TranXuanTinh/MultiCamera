# Multi-Camera Video Analytics Framework (NVIDIA DeepStream & Yocto Linux)

Production-grade real-time Multi-Camera Video Analytics, Object Detection, Tracking, and Spatial Rule Evaluation framework engineered for NVIDIA Jetson platforms (Jetson Nano, Xavier NX, Orin Nano) using the **NVIDIA DeepStream SDK**, **Yocto Linux**, and **C++17**.

---

## 🌟 Key Features

* **Multi-Source Video Ingestion**:
  * Simultaneous capture from Dual MIPI CSI-2 sensors (`nvarguscamerasrc` via ISP).
  * High-throughput network IP camera ingestion (RTSP H.264/H.265 via hardware `nvv4l2decoder`).
  * Synthetic video files and test pattern feeds for offline benchmarking.
* **Hardware-Accelerated DeepStream Pipeline**:
  * **Zero-Copy Memory**: All frames reside in unified NVMM (NVidia Memory Management) buffers from capture through decoding, scaling, and inference.
  * **Dynamic Multi-Stream Batching**: `nvstreammux` collects $N$ asynchronous camera streams into synchronized GPU inference batches.
  * **Batched Primary GIE**: TensorRT-accelerated YOLOv8 (`nvinfer`) operating in FP16 / INT8 modes.
  * **Multi-Stream Object Tracking**: Hardware `nvtracker` maintaining persistent tracking IDs per camera channel without inter-channel ID collisions.
  * **Pad Probe Metadata Extraction**: High-performance, zero-overhead metadata extraction (`GST_PAD_PROBE_TYPE_BUFFER`) inspecting `NvDsBatchMeta` without copying frame pixel memory.
* **Per-Camera Spatial Rule Engine**:
  * Multi-channel zone intrusion detection (ray-casting point-in-polygon algorithm) bound to specific camera IDs (`cam_01`, `cam_02`) or wildcard (`*`).
  * Directional line-crossing counting with compound keying `[camera_id, track_id]` preventing cross-camera ID collision.
  * Per-camera alert cooldown and automated state purge (`resetCameraState`) upon camera reconnect.
* **Dual-Mode Architectural Execution**:
  * **Target Jetson Mode (`ENABLE_DEEPSTREAM=ON`)**: Full NVIDIA hardware acceleration via DeepStream SDK GStreamer plugins.
  * **Host Simulation Mode (`MockDeepStreamHAL`)**: 100% functional host simulation running on standard x86 Linux / Windows without Jetson hardware or DeepStream SDK, enabling comprehensive CI/CD testing.
* **Industrial Embedded Foundation & Verified Safety**:
  * Multi-topic Paho MQTT client publishing structured telemetry (`edge/<device>/<camera_id>/detections`) and per-camera runtime telemetry (`CameraRuntimeMetrics`).
  * Embedded SQLite3 database with multi-camera indexing (`camera_id`, `source_id`) and bounded memory ring-buffer queue (1,000 records).
  * Deterministic 4-phase safe shutdown order (Ingress $\to$ Queue Drain $\to$ DB/MQTT $\to$ Watchdog release `'V'`).
  * Tegra hardware watchdog integration and dynamic thermal PWM fan control.
  * Dedicated Yocto Project BSP layer (`meta-jetson-ai-detect`).


---

## 📁 Repository Structure

```
├── docs/
│   ├── system_requirements/     # Hardware specs, multi-CSI, RTSP bandwidth, power & thermal
│   ├── software_requirements/   # Yocto Dunfell, DeepStream SDK 6.0+, GStreamer plugins
│   ├── SETUP_GUIDE.md           # Step-by-step setup guide (Host PC, Jetson Native, Yocto)
│   └── design/
│       ├── system_design.md     # 3-Tier architecture, NVMM zero-copy flow, RAM budget
│       ├── software_design.md   # IDeepStreamHAL interface, DI, and class hierarchy
│       ├── high_level_design.md # Subsystem contracts and DeepStream pad probe flow
│       ├── low_level_design.md  # NvDsBatchMeta mapping, Ray-casting math, SQLite schema
│       ├── threading_model.md   # Concurrency architecture: DeepStream threads & GStreamer bus
│       └── system_workflow.md   # Per-batch CPU/GPU/VIC/NVDEC lifecycle and timing budget
│
├── config/
│   ├── app_config.yaml          # Multi-camera sources, DeepStream engine, MQTT, storage
│   ├── rules.yaml               # Per-camera zone polygon & line crossing rules
│   ├── deepstream_pgie_config.txt    # Primary GIE (YOLOv8) nvinfer configuration
│   └── deepstream_tracker_config.txt # nvtracker multi-stream configuration
│
├── include/
│   ├── app/                     # Tier 1: types.h, edge_ai_manager.h, pipeline.h, rule_engine.h
│   │   └── rules/               # i_rule.h, zone_rule.h, counting_rule.h
│   ├── service/                 # Tier 2: camera, inference, tracker, pre/postprocessor,
│   │                            #         kalman/hungarian, event_bus, mqtt, storage, health, config, log
│   └── hal/                     # Tier 3: i_deepstream_hal, i_camera_hal, i_gpu_hal,
│                                #         i_network_hal, i_storage_hal, i_sensor_hal, i_watchdog_hal
│
├── src/
│   ├── app/                     # Tier 1 implementation & main.cpp entry point
│   ├── service/                 # Tier 2 platform-independent services
│   └── hal/
│       ├── jetson/              # Tier 3 Jetson HALs (DeepStream, GStreamer, CUDA, SQLite, sysfs)
│       └── mock/                # Tier 3 Mock HALs for 100% Host PC testing without hardware
│
├── tests/                       # Unit tests for Multi-Camera Pipeline and Per-Camera RuleEngine
├── yocto/meta-jetson-ai-detect/ # Custom Yocto layer (recipes for app and system image)
├── systemd/                     # Production systemd daemon unit file with hardware watchdog
└── CMakeLists.txt               # Unified CMake build definition with ENABLE_DEEPSTREAM option
```

---

## 🚀 Quickstart

### 1. Host PC Testing (No Jetson Hardware or DeepStream SDK Needed)

Run the entire multi-camera application and automated test suite on your standard x86 Linux or Windows development machine:

```bash
mkdir -p build && cd build
cmake .. -DBUILD_TESTS=ON
cmake --build . -j$(nproc)

# Run automated tests:
ctest --output-on-failure

# Launch multi-camera simulator:
./edge-ai-manager --config ../config/app_config.yaml --mock
```

### 2. Native Execution on NVIDIA Jetson

When building on an NVIDIA Jetson board equipped with DeepStream SDK:

```bash
mkdir -p build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_DEEPSTREAM=ON \
    -DENABLE_PAHO_MQTT=ON \
    -DENABLE_SQLITE3=ON \
    -DENABLE_YAML_CPP=ON

cmake --build . -j4
sudo ./edge-ai-manager --config ../config/app_config.yaml
```

### 3. Yocto Project Embedded OS Build

To generate a production-ready, stripped-down Linux operating system:

1. Clone Poky Dunfell and dependencies:
   ```bash
   git clone -b dunfell git://git.yoctoproject.org/poky
   git clone -b dunfell-l4t-r32.7.x https://github.com/OE4T/meta-tegra.git
   git clone -b dunfell https://github.com/openembedded/meta-openembedded.git
   ```
2. Integrate `meta-jetson-ai-detect` into `conf/bblayers.conf`.
3. Set `MACHINE = "jetson-nano"` (or `jetson-orin-nano`) in `conf/local.conf`.
4. Compile the final image:
   ```bash
   bitbake jetson-ai-image
   ```
5. Flash the generated `.sdcard` image to your target MicroSD card or eMMC storage.

---

## 📊 Pipeline Dataflow

```
[Cam 0: CSI ] -> nvarguscamerasrc -\
                                    +-> nvstreammux -> nvinfer (PGIE) -> nvtracker -> [Pad Probe] -> fakesink
[Cam 1: RTSP] -> rtspsrc -> nvdec -/    (Batch N)    (Batched TensorRT)  (Multi-ID)        |
                                                                                           v
                                                                                   PipelineResult
                                                                                           |
                                                                             +-------------+-------------+
                                                                             |                           |
                                                                             v                           v
                                                                     RuleEngine (cam_id)         StorageService
                                                                             |                   (Indexed SQLite)
                                                                             v
                                                                        MqttService
                                                                    (edge/.../alerts)
```

---

## 📜 Documentation Index

* [System Design](file:///media/tinhtran/01D85BC599D1D460/FreeLancer/MultiCamera/docs/design/system_design.md)
* [High-Level Design](file:///media/tinhtran/01D85BC599D1D460/FreeLancer/MultiCamera/docs/design/high_level_design.md)
* [Software Design](file:///media/tinhtran/01D85BC599D1D460/FreeLancer/MultiCamera/docs/design/software_design.md)
* [Low-Level Design](file:///media/tinhtran/01D85BC599D1D460/FreeLancer/MultiCamera/docs/design/low_level_design.md)
* [Threading Model](file:///media/tinhtran/01D85BC599D1D460/FreeLancer/MultiCamera/docs/design/threading_model.md)
* [System Workflow](file:///media/tinhtran/01D85BC599D1D460/FreeLancer/MultiCamera/docs/design/system_workflow.md)
* [Hardware Setup & Deployment Guide](file:///media/tinhtran/01D85BC599D1D460/FreeLancer/MultiCamera/docs/SETUP_GUIDE.md)
* [System Requirements](file:///media/tinhtran/01D85BC599D1D460/FreeLancer/MultiCamera/docs/system_requirements/README.md)
* [Software Requirements](file:///media/tinhtran/01D85BC599D1D460/FreeLancer/MultiCamera/docs/software_requirements/README.md)
