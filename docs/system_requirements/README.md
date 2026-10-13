# System Requirements Specification (SRS)
## Project: Multi-Camera Video Analytics Framework based on NVIDIA DeepStream & Yocto Linux

---

## 1. Hardware Requirements

### 1.1 Target Hardware Platforms

The software is architected to run across the NVIDIA Jetson family with dynamic scaling according to channel count:

| Specification | NVIDIA Jetson Nano 4GB (DevKit B01) | NVIDIA Jetson Orin Nano (4GB / 8GB) |
|---|---|---|
| **Target Scale** | **1 – 4 Camera Streams** (720p @ 15–30 fps) | **4 – 8+ Camera Streams** (1080p @ 30 fps) |
| **CPU Architecture** | Quad-core ARM® Cortex®-A57 @ 1.43 GHz | 6-core ARM® Cortex®-A78AE v8.2 @ 1.5 GHz |
| **GPU Architecture** | 128-core NVIDIA Maxwell™ @ 921 MHz (472 GFLOPS) | 512 / 1024-core NVIDIA Ampere™ (20 – 40 TOPS) |
| **Unified Memory (RAM)** | 4GB 64-bit LPDDR4 @ 1600 MHz (25.6 GB/s) | 4GB / 8GB 128-bit LPDDR5 (68 – 102 GB/s) |
| **Hardware Video Decoders** | 1x 4K30 / 2x 1080p60 / 4x 1080p30 (H.264/H.265 NVDEC) | Up to 1x 4K60 / 4x 4K30 / 8x 1080p30 (NVDEC) |
| **ISP Pipeline** | Dual ISP engines (up to 1.4 Gpix/s) | 2x NVCSI / ISP engines (up to 1.5 Gpix/s) |
| **Power Supply** | 5V / 4A DC Barrel Jack (20W) | 9V – 20V DC Barrel Jack (7W – 15W mode) |
| **Thermal Solution** | 40mm x 40mm PWM 4-pin Fan | 4-pin Active Heatpipe Fan assembly |

### 1.2 Multi-Camera Physical Interfaces

1. **Dual MIPI CSI-2 Connectors (On-board CAM0 and CAM1)**
   * **Physical Connector**: Dual 15-pin FPC connectors with 2-lane MIPI CSI-2 per port.
   * **Supported Image Sensors**:
     * Sony IMX219 (8.08 MP, $3280 \times 2464$, default Jetson Nano kernel driver).
     * Sony IMX477 (12.3 MP High Quality Camera).
   * **Bandwidth & Processing**: Up to 1.5 Gbps per lane; processed zero-copy through the Tegra hardware ISP via `nvarguscamerasrc`.

2. **Network IP Cameras (RTSP over Ethernet)**
   * **Physical Interface**: RJ45 Gigabit Ethernet port connected to a dedicated IP camera PoE Switch.
   * **Network Bandwidth Budget**:
     * Per Camera (1080p H.264 @ 25 fps, 4 Mbps VBR): $\approx 4\text{ Mbps}$.
     * 4 Concurrent RTSP Cameras: $\approx 16\text{ – }20\text{ Mbps}$ steady-state traffic.
     * 8 Concurrent RTSP Cameras: $\approx 32\text{ – }40\text{ Mbps}$ steady-state traffic.
   * **Cabling**: Cat5e or Cat6 shielded twisted pair (STP) Ethernet cabling.

3. **Local Video File / Synthetic Stream (Simulation & Testing)**
   * **Storage Speed**: Sequential read $\ge 50\text{ MB/s}$ from NVMe SSD or UHS-I U3 MicroSD card to feed multiple parallel video files (`file://...`) into the pipeline.

### 1.3 Power Supply & Power Budget

* **Power Jumper J48 Configuration (Jetson Nano)**:
  * Header J48 **must be shorted** with a standard 2.54mm jumper cap.
  * Disables Micro-USB power input and activates the **5.5mm OD / 2.1mm ID Barrel Jack (J25)**.
* **Power Requirement**:
  * Regulated **5V / 4A DC (20W)** power adapter with low ripple ($< 50\text{ mV}$).
  * Power Mode set to **10W MAXN** (`sudo nvpmodel -m 0`) to prevent CPU/GPU throttling during multi-channel NVDEC and TensorRT batch inference.

### 1.4 Thermal & Cooling Requirements

Multi-stream DeepStream pipelines run the hardware video decoder (NVDEC), GPU TensorRT engine, and memory controller concurrently.
* **Active Cooling**: 40mm $\times$ 40mm 5V PWM fan attached to header J15 is mandatory.
* **Thermal Ceiling**: Die temperature must be maintained below $75^\circ\text{C}$ (`/sys/devices/virtual/thermal/thermal_zone0/temp`).
* **Dynamic Fan Control**: The system's `HealthService` monitors thermal zones and dynamically regulates PWM duty cycle between 40% and 100%.

### 1.5 Storage & Persistent Data

* **Operating System & Models**: MicroSD card (UHS-I U3, A2 rating, $\ge 64\text{ GB}$) or NVMe SSD (M.2 Key M via carrier adapter).
* **SQLite Event Database**: Ring-buffer storage partitioned per camera channel (`camera_id`). Maximum DB size is capped at 500 MB with automatic FIFO purging.

---

## 2. Host Build Machine Requirements

To cross-compile the Yocto Linux image or run the complete multi-camera test suite:
* **Operating System**: Ubuntu 20.04 LTS or 22.04 LTS (x86_64).
* **CPU**: 8+ cores (AMD Ryzen 7 / Intel Core i7 or higher).
* **RAM**: 32 GB minimum (recommended for BitBake parallel compilation).
* **Disk Space**: $\ge 350\text{ GB}$ free SSD space (ext4 file system).
* **Host Simulation Capabilities**:
  * CMake $\ge 3.16$
  * GCC $\ge 9.3$ or Clang $\ge 11$ with C++17 support
  * OpenCV $\ge 4.5.0$
  * GStreamer 1.16+ development packages
