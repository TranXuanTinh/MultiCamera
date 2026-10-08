# Software Requirements Specification (SRS)
## Project: Embedded AI Object Detection on Jetson Nano 4GB (Yocto Linux)

---

## 1. Operating System & BSP (Yocto Linux Stack)

The system is built entirely from source using the Yocto Project / OpenEmbedded build framework instead of standard Ubuntu 18.04 desktop image, providing minimal footprint, fast boot times, and production reliability.

### 1.1 Yocto Release & Layers Configuration

| Layer | Repository URL | Branch | Purpose |
|---|---|---|---|
| **poky** | `git://git.yoctoproject.org/poky` | `dunfell` (3.1 LTS) | Core build engine, BitBake, minimal rootfs, toolchain (`aarch64-poky-linux-gcc 9.3+`) |
| **meta-tegra** | `https://github.com/OE4T/meta-tegra.git` | `dunfell-l4t-r32.7.x` | NVIDIA Tegra BSP, L4T kernel 4.9.299, proprietary drivers, CUDA, TensorRT |
| **meta-openembedded / meta-oe** | `https://github.com/openembedded/meta-openembedded.git` | `dunfell` | General utility packages, SQLite, yaml-cpp |
| **meta-openembedded / meta-networking** | (same repository) | `dunfell` | Network utilities, Paho MQTT support |
| **meta-openembedded / meta-multimedia** | (same repository) | `dunfell` | GStreamer 1.16+ plugins |
| **meta-openembedded / meta-python** | (same repository) | `dunfell` | Python3 support for tooling |
| **meta-jetson-ai-detect** | Local / Git repository | `main` | Custom application recipes, configuration files, systemd unit files |

### 1.2 NVIDIA JetPack 4.6.x (L4T R32.7.x) Software Stack

| Component | Target Version | Delivery Method in Yocto |
|---|---|---|
| **Board Support Package (L4T)** | R32.7.1 / R32.7.2 | `meta-tegra` recipes |
| **Linux Kernel** | 4.9.299-tegra | Built via `linux-tegra_4.9.bb` |
| **CUDA Toolkit** | 10.2.300 | `cuda-toolkit-10-2` / `cuda-libraries-10-2` |
| **cuDNN Library** | 8.2.1 | `cudnn_8.2.1` recipe in meta-tegra |
| **TensorRT Runtime** | 8.2.1 | `tensorrt_8.2.1` recipe in meta-tegra |
| **Argus Camera API** | R32.7.x | `tegra-libraries-multimedia-v4l` |
| **GStreamer Jetson Plugins** | 1.14 / 1.16 | `gstreamer1.0-plugins-nvarguscamerasrc`, `gstreamer1.0-plugins-nvvidconv` |

---

## 2. Core Application Libraries & Dependencies

All libraries are linked and compiled natively in C++17.

| Library | Version | License | Layer | Role in Architecture |
|---|---|---|---|---|
| **C++ Standard** | ISO C++17 | - | All | Modern memory semantics (`std::unique_ptr`, `std::any`, `std::optional`, `std::filesystem`) |
| **TensorRT C++ API** | 8.2.1 | NVIDIA Proprietary | HAL / Service | Engine deserialization, GPU execution context, binding management |
| **CUDA Runtime API** | 10.2 | NVIDIA Proprietary | HAL | Device memory allocation (`cudaMalloc`), host-device memory transfers |
| **OpenCV** | 4.5.x | Apache 2.0 | Service | Image data structure (`cv::Mat`), color conversion, matrix manipulations |
| **GStreamer** | 1.16+ | LGPL 2.1 | HAL | Zero-copy CSI camera pipeline with hardware acceleration |
| **Eclipse Paho MQTT C++** | 1.3+ (Paho C 1.3.12+) | EPL 2.0 | HAL / Service | Asynchronous MQTT publisher client, persistent offline queuing |
| **SQLite3** | 3.36+ | Public Domain | HAL / Service | Local embedded database, ring-buffer event and detection storage |
| **nlohmann/json** | 3.11+ | MIT | Service | Modern C++ JSON serialization for MQTT telemetry payloads |
| **spdlog** | 1.12+ | MIT | Service | Fast thread-safe logging with stdout, file, and systemd journal sinks |
| **yaml-cpp** | 0.7+ | MIT | Service | Parsing and validating YAML configuration files (`app_config.yaml`, `rules.yaml`) |

---

## 3. System Daemons & OS Services

* **systemd**: Init system (PID 1) providing service life-cycle management, auto-restart on unexpected crashes (`Restart=on-failure`), and watchdog keepalive.
* **systemd-journald**: High-performance structured system logger.
* **NetworkManager**: Automatic Ethernet and Wi-Fi interface management with DHCP client.
* **Hardware Watchdog (`/dev/watchdog`)**: Kernel driver for Tegra watchdog.

---

## 4. Cloud & External Infrastructure Requirements

* **MQTT Broker**:
  * Protocols: MQTT v3.1.1 / MQTT v5.0 over TCP (port 1883) or TLS (port 8883).
  * Recommended: Eclipse Mosquitto (on-premise / edge) or AWS IoT Core / HiveMQ Cloud.
* **Time Synchronization**:
  * System time must be synchronized via NTP (`systemd-timesyncd`) to guarantee accurate ISO 8601 timestamps on detection payloads.

