# Embedded AI Object Detection on Jetson Nano 4GB (Yocto Linux & C++17)

Production-grade real-time Object Detection and Tracking system engineered specifically for NVIDIA Jetson Nano 4GB (JetPack 4.6.x / L4T R32.7.x) using Yocto Linux, TensorRT, and C++17.

---

## 📁 Repository Structure

```
├── docs/
│   ├── system_requirements/     # Hardware, target SoC, power, thermal, and host requirements
│   ├── software_requirements/   # Yocto Dunfell, meta-tegra, L4T R32.7.x, libraries
│   └── design/
│       ├── system_design.md     # 3-Layer architecture, thread model, zero-copy data flow
│       ├── software_design.md   # Decomposition, Dependency Injection, class hierarchies
│       ├── high_level_design.md # Subsystem contracts and responsibilities
│       └── low_level_design.md  # Data structures, math algorithms (letterbox, ray casting)
│
├── config/
│   ├── app_config.yaml          # Camera, TensorRT, MQTT, storage, and health configs
│   └── rules.yaml               # Polygon safety zone and counting line definitions
│
├── models/
│   └── coco_classes.txt         # Standard 80 COCO class labels
│
├── include/
│   ├── app/                     # Tier 1: types.h, edge_ai_manager.h, pipeline.h, rule_engine.h
│   ├── service/                 # Tier 2: camera, inference, tracker, event_bus, mqtt, storage, health
│   └── hal/                     # Tier 3: i_camera_hal, i_gpu_hal, i_network_hal, i_storage_hal, i_sensor_hal, i_watchdog_hal
│
├── src/
│   ├── app/                     # Tier 1 implementation & main.cpp entry point
│   ├── service/                 # Tier 2 platform-independent services
│   └── hal/
│       ├── jetson/              # Tier 3 Jetson Nano concrete HALs (CUDA, TensorRT, GStreamer, sysfs)
│       └── mock/                # Tier 3 Mock HALs for 100% Host PC testing without hardware
│
├── tests/                       # Unit tests for Pipeline, ByteTrack Tracker, and RuleEngine
├── yocto/meta-jetson-ai-detect/ # Custom Yocto layer (recipes for app and system image)
├── systemd/                     # Production systemd daemon unit file with hardware watchdog
└── CMakeLists.txt               # Unified CMake build definition
```

---

## 🚀 Quickstart

### 1. Host PC Testing (No Jetson hardware required)
You can compile and test the full application logic on any Linux / Windows x86 machine using the built-in **Mock HAL**:

```bash
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON
cmake --build .
ctest --output-on-failure

# Run application in Mock mode:
./edge-ai-manager --config ../config/app_config.yaml --mock
```

### 2. Building Yocto Image for Jetson Nano
1. Clone Poky Dunfell and dependencies:
   ```bash
   git clone -b dunfell git://git.yoctoproject.org/poky
   git clone -b dunfell-l4t-r32.7.x https://github.com/OE4T/meta-tegra.git
   git clone -b dunfell https://github.com/openembedded/meta-openembedded.git
   ```
2. Initialize build environment:
   ```bash
   source poky/oe-init-build-env build
   ```
3. Add layers in `conf/bblayers.conf`:
   ```text
   BBLAYERS += " \
     ${TOPDIR}/../meta-tegra \
     ${TOPDIR}/../meta-openembedded/meta-oe \
     ${TOPDIR}/../meta-openembedded/meta-networking \
     ${TOPDIR}/../meta-openembedded/meta-multimedia \
     ${TOPDIR}/../meta-openembedded/meta-python \
     /path/to/Object-Detection-With-Yolov8-In-Jetson-Nano/yocto/meta-jetson-ai-detect \
   "
   ```
4. Set machine in `conf/local.conf`:
   ```text
   MACHINE = "jetson-nano"
   ```
5. Build target production image:
   ```bash
   bitbake jetson-ai-image
   ```
6. Flash output `.tegraflash.zip` / `.sdcard` to MicroSD card.

