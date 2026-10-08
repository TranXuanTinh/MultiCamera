# System Requirements Specification (SRS)
## Project: Embedded AI Object Detection on Jetson Nano 4GB (Yocto Linux)

---

## 1. Hardware Requirements

### 1.1 Target Device: NVIDIA Jetson Nano 4GB Developer Kit (Carrier Board B01)
* **Processor (SoC)**: NVIDIA Tegra X1 (T210)
  * **CPU**: Quad-core ARM® Cortex®-A57 MPCore processor @ 1.43 GHz (L1: 48KB I-cache, 32KB D-cache per core; L2: 2MB shared cache).
  * **GPU**: 128-core NVIDIA Maxwell™ architecture GPU with 16 Streaming Multiprocessors (SMs) @ 921 MHz.
* **Unified Memory (RAM)**: 4GB 64-bit LPDDR4 @ 1600MHz (bandwidth: 25.6 GB/s). Shared dynamically between CPU and GPU.
* **Storage Interface**:
  * MicroSD Card Slot: UHS-I bus speed class, recommended Class 10 U3 / V30 / A2 (min 64GB).
  * Optional / Production: NVMe SSD via M.2 Key M slot or USB 3.0 bridge for endurance and high write speeds.
* **Power Supply**:
  * Connector: 5.5mm OD / 2.1mm ID Barrel Jack (J25), Center Positive.
  * Rating: **5V / 4A DC regulated power supply (20W)**.
  * Jumper: J48 Power Select header **must be shorted** with a 2.54mm jumper to disable Micro-USB power and enable 5V/4A Barrel Jack.
  * Power Mode: Configured to **10W MAXN mode** (`nvpmodel -m 0`) to allow maximum CPU/GPU clock rates.
* **Thermal Management**:
  * 40mm x 40mm x 20mm PWM Fan (5V 4-pin connected to J15 fan header).
  * Active PWM control enabled via Linux kernel sysfs (`/sys/devices/pwm-fan/target_pwm`).

### 1.2 Camera Interfaces
* **Primary Interface: MIPI CSI-2 (Camera Serial Interface)**
  * Physical Connector: Dual 15-pin MIPI CSI-2 camera connectors (CAM0 / CAM1).
  * Supported Sensors: Sony IMX219 (8MP, default Jetson Nano driver) or Sony IMX477 (12.3MP HQ camera).
  * Data Lanes: 2-lane MIPI CSI-2 configuration.
  * Hardware Processing: On-chip Image Signal Processor (ISP) handles demosaicing, auto-exposure (AE), auto-white-balance (AWB), lens shading correction with near-zero CPU load.
* **Secondary / Fallback Interface: USB 3.0 UVC Camera**
  * Protocol: USB Video Class (UVC 1.1/1.5) via standard V4L2 kernel driver (`/dev/video*`).
  * Capture Formats: YUYV, MJPEG @ 720p 30fps.

### 1.3 Network Interfaces
* **Wired Ethernet**: Integrated Gigabit Ethernet port (Realtek RTL8111H PHY, RJ45) with auto MDI/MDI-X.
* **Wireless (Optional)**: USB Wi-Fi dongle (802.11ac/b/g/n, e.g., RTL8812AU or RTL8188EUS) or M.2 Key E wireless module (Intel Dual Band Wireless-AC 8265).
* **Protocol Requirement**: Outbound TCP port 1883 (MQTT standard) or 8883 (MQTT over TLS) to reach MQTT broker.

### 1.4 Hardware Watchdog
* Hardware Watchdog timer integrated into Tegra PMIC / SoC (`/dev/watchdog`).
* Timeout threshold: 60 seconds. Must be kicked periodically at least every 30 seconds.

---

## 2. Host Build Machine Requirements

To cross-compile the Yocto Linux BSP image and build recipes:
* **Operating System**: Ubuntu 18.04 LTS or Ubuntu 20.04 LTS (x86_64, native installation strongly recommended).
* **CPU**: 8+ cores / 16 threads (e.g., AMD Ryzen 7 / Intel Core i7 or higher).
* **RAM**: 32 GB minimum (16 GB will require swap and build may fail during BitBake webkit/compiler steps).
* **Disk Space**: At least 350 GB free SSD space (ext4 file system).
* **Internet**: High-speed broadband connection (BitBake downloads ~30GB of source tarballs and Git repositories).

