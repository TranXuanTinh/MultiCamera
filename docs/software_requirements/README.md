# Software Requirements Specification (SRS)
## Project: Multi-Camera Video Analytics Framework based on NVIDIA DeepStream & Yocto Linux (C++17)

---

## 1. Operating System & BSP (Yocto Linux Stack)

The system is built from source using the Yocto Project / OpenEmbedded build framework, producing a minimal, high-reliability embedded Linux image without desktop GUI overhead.

### 1.1 Yocto Release & Layers Configuration

| Layer | Repository URL | Branch | Purpose |
|---|---|---|---|
| **poky** | `git://git.yoctoproject.org/poky` | `dunfell` (3.1 LTS) / `kirkstone` (4.0 LTS) | Core build engine, BitBake, minimal rootfs, toolchain (`aarch64-poky-linux-gcc`) |
| **meta-tegra** | `https://github.com/OE4T/meta-tegra.git` | `dunfell-l4t-r32.7.x` (Nano) or `kirkstone-l4t-r35.x` (Orin) | NVIDIA Tegra BSP, L4T kernel, proprietary drivers, CUDA, TensorRT |
| **meta-openembedded / meta-oe** | `https://github.com/openembedded/meta-openembedded.git` | `dunfell` / `kirkstone` | General utility packages, SQLite, yaml-cpp |
| **meta-openembedded / meta-networking** | (same repository) | `dunfell` / `kirkstone` | Network utilities, Paho MQTT client |
| **meta-openembedded / meta-multimedia** | (same repository) | `dunfell` / `kirkstone` | GStreamer 1.16+ base & good plugins |
| **meta-openembedded / meta-python** | (same repository) | `dunfell` / `kirkstone` | Python3 support for export and conversion tools |
| **meta-jetson-ai-detect** | Local repository (`yocto/`) | `main` | Application recipes, DeepStream pipeline configs, systemd unit files |

### 1.2 NVIDIA JetPack & DeepStream Software Stack

| Component | Target Version | Delivery Method | Role in Architecture |
|---|---|---|---|
| **NVIDIA DeepStream SDK** | 6.0.1 (JetPack 4.6.x) / 6.3+ (JetPack 5.1+) | `deepstream-6.0` package / recipes | Hardware-accelerated multi-stream batching, PGIE inference, tracking, tiling, and metadata management |
| **TensorRT Runtime** | 8.2.1+ | `tensorrt` in meta-tegra | High-performance INT8 / FP16 deep learning inference runtime |
| **CUDA Toolkit** | 10.2 / 11.4+ | `cuda-toolkit` / `cuda-libraries` | GPU compute backend and memory management (`cudaMallocManaged`, CUDA streams) |
| **Argus Camera API** | L4T R32.7.x+ | `tegra-libraries-multimedia-v4l` | Low-latency MIPI CSI-2 multi-sensor capture via on-chip ISP |
| **GStreamer Core & Plugins** | 1.16.x / 1.20.x | `gstreamer1.0` / meta-multimedia | Multimedia graph execution engine, pipeline elements, and pad probes |
| **NVIDIA GStreamer Plugins** | Integrated with DeepStream & L4T | NVIDIA BSP | `nvarguscamerasrc`, `rtspsrc`, `nvv4l2decoder`, `nvstreammux`, `nvinfer`, `nvtracker`, `nvmultistreamtiler`, `nvdsosd` |

---

## 2. DeepStream Framework Components & Plugins

The multi-camera pipeline requires the following specialized GStreamer / DeepStream plugins:

| Plugin Element | Package / Provider | Purpose |
|---|---|---|
| **`nvarguscamerasrc`** | `tegra-libraries-multimedia` | Captures raw frames from MIPI CSI-2 sensors (Sony IMX219/IMX477) into NVMM (NVidia Memory Management) memory blocks |
| **`rtspsrc` + `rtph264depay`** | `gstreamer1.0-plugins-good` | Receives network H.264/H.265 RTSP streams from remote IP cameras with automatic jitter buffering |
| **`nvv4l2decoder`** | `tegra-libraries-multimedia` | Hardware-accelerated NVDEC decoding of RTSP H.264/H.265 streams directly to NVMM memory |
| **`nvstreammux`** | DeepStream SDK (`libnvdsgst_multistream.so`) | Batches $N$ asynchronous camera streams into a single synchronized batched NVMM buffer with uniform width/height/framerate |
| **`nvinfer`** | DeepStream SDK (`libnvdsgst_infer.so`) | Executes batched Primary GIE (YOLOv8) inference on GPU using TensorRT; attaches `NvDsBatchMeta` |
| **`nvtracker`** | DeepStream SDK (`libnvdsgst_tracker.so`) | Multi-stream object tracking (NVTracker / NvSORT / NvDeepSORT) with persistent `object_id` assignment |
| **`nvmultistreamtiler`** | DeepStream SDK (`libnvdsgst_multistreamtiler.so`) | Composites $N$ video streams into a single $2 \times 2$ or $M \times K$ mosaic grid for optional local display / debugging |
| **`nvdsosd`** | DeepStream SDK (`libnvdsgst_osd.so`) | Hardware-accelerated on-screen display of bounding boxes, labels, and tracking lines via GPU/VIC |

---

## 3. Core Application Libraries & Dependencies

All core application modules are written in ISO C++17 with clean separation into App, Service, and HAL tiers:

| Library | Minimum Version | License | Tier | Role in Architecture |
|---|---|---|---|---|
| **C++ Standard** | ISO C++17 | - | All | Modern memory semantics (`std::unique_ptr`, `std::shared_ptr`, `std::optional`, `std::variant`, `std::string_view`) |
| **NVIDIA DeepStream API** | 6.0+ | NVIDIA Proprietary | HAL / Jetson | `gstnvdsmeta.h`, `nvdsmeta.h`, `nvds_batch_meta` metadata traversal and buffer inspection |
| **GStreamer C API** | 1.16+ | LGPL 2.1 | HAL / Jetson | Pipeline construction, bus watch message loop, pad probe registration (`gst_pad_add_probe`) |
| **OpenCV** | 4.5.x | Apache 2.0 | App / Rules / Testing | Geometry calculations (`cv::Point2f`, `cv::Rect2f`), point-in-polygon tests, synthetic test frame rendering |
| **Eclipse Paho MQTT C++** | 1.3+ (Paho C 1.3.12+) | EPL 2.0 | Service / HAL | Multi-topic asynchronous telemetry publishing with camera-tagged event topics |
| **SQLite3** | 3.36+ | Public Domain | Service / HAL | Multi-camera indexed persistent storage (`camera_id`, `source_id`, `detections`, `alerts`) |
| **nlohmann/json** | 3.11+ | MIT | Service | Structured JSON serialization with per-camera telemetry payloads |
| **spdlog** | 1.12+ | MIT | All | High-throughput, thread-safe asynchronous logging |
| **yaml-cpp** | 0.7+ | MIT | Service | Dynamic parsing of multi-camera source lists (`cameras:`) and DeepStream engine parameters |

---

## 4. System Daemons & OS Services

* **systemd**: Init system (PID 1) providing service life-cycle management, auto-restart on unexpected crashes (`Restart=always`, `RestartSec=5`), and hardware watchdog integration.
* **systemd-journald**: Structured system logging with rate limiting.
* **NetworkManager**: High-reliability network interface management (Gigabit Ethernet + Wi-Fi) with automatic RTSP camera link re-establishment.
* **Hardware Watchdog (`/dev/watchdog`)**: Tegra hardware watchdog timer reset via `HealthService` every 30 seconds.

---

## 5. Network & External Infrastructure Requirements

* **IP Camera Streams (RTSP)**:
  * Protocols: RTSP over TCP / UDP (RTP/RTCP).
  * Video Codecs: H.264 (Baseline, Main, High profile) or H.265 (HEVC).
  * Resolution / FPS: 720p @ 15–30 fps or 1080p @ 15–30 fps per camera.
* **MQTT Broker**:
  * Protocols: MQTT v3.1.1 / MQTT v5.0 over TCP (port 1883) or TLS (port 8883).
  * Supported Brokers: Eclipse Mosquitto (edge/on-premise), EMQX, AWS IoT Core, Azure IoT Hub.
* **Time Synchronization**:
  * Mandatory NTP synchronization (`systemd-timesyncd`) to guarantee accurate UTC / ISO 8601 timestamps across all batched camera events.
