# 📖 Hướng Dẫn Cài Đặt & Triển Khai (Setup Guide)
## Dự Án: Embedded AI Object Detection Trên Jetson Nano 4GB (Yocto Linux & C++17)

Tài liệu này hướng dẫn chi tiết từng bước cách thiết lập phần cứng, cài đặt môi trường, biên dịch và triển khai hệ thống theo 3 kịch bản:
1. **Kịch bản 1**: Chạy thử nghiệm và Unit Test trên **Host PC (Windows / Linux x86)** với **Mock HAL** (không cần board Jetson).
2. **Kịch bản 2**: Biên dịch và chạy trực tiếp trên **Jetson Nano (JetPack 4.6.x / Ubuntu)** để test trước khi build Yocto.
3. **Kịch bản 3**: Build hệ điều hành nhúng hoàn chỉnh bằng **Yocto Project (Dunfell)** và nạp vào thẻ nhớ.

---

## 🔌 PHẦN 1: Thiết Lập Phần Cứng Jetson Nano 4GB (Hardware Setup)

Trước khi cấp nguồn, hãy đảm bảo các kết nối phần cứng chính xác:

```
                  +-----------------------------------+
                  |        Jetson Nano 4GB B01        |
                  |                                   |
[Nguồn 5V/4A] --->| J25 (Barrel Jack)                 |
[Jumper J48]  --->| Short pin (Disable Micro-USB)     |
[CSI Camera]  --->| J13 (CAM0 Connector - IMX219)     |
[Quạt 5V PWM] --->| J15 (4-pin PWM Fan Header)        |
[Mạng LAN]    --->| RJ45 Gigabit Ethernet             |
                  +-----------------------------------+
```

### 1. Cấu hình Nguồn 5V/4A Barrel Jack
* **Bắt buộc**: Dùng nguồn ngoài qua cổng Barrel Jack **5V/4A DC (J25)**.
* **Cắm Jumper J48**: Tìm header 2 chân J48 (nằm ngay dưới cổng Camera CAM0). **Nối tắt (short) 2 chân này bằng jumper 2.54mm** để ngắt nguồn cổng Micro-USB và kích hoạt cổng Barrel Jack.
* *Lưu ý*: Nếu dùng nguồn Micro-USB 5V/2A, khi GPU chạy TensorRT FP16 sẽ lập tức sụt áp gây sập nguồn (Brownout Reset).

### 2. Kết nối Camera MIPI CSI-2 (Sony IMX219)
* Mở khóa chốt nhựa trên cổng **J13 (CAM0)** bằng cách kéo nhẹ lên.
* Cắm cáp ribbon của camera vào: **mặt có dải tiếp xúc màu bạc quay về phía tản nhiệt (SoC)**, mặt màu xanh quay ra ngoài cổng kết nối.
* Đẩy chốt nhựa xuống để cố định cáp.

### 3. Kết nối Quạt tản nhiệt PWM (J15)
* Cắm quạt 4-pin 5V vào header J15. Quạt sẽ được điều khiển tự động bởi `HealthService` qua PWM kernel.

---

## 💻 PHẦN 2: Kịch Bản 1 — Chạy Thử Nghiệm Trên Host PC Với Mock HAL

Kiến trúc 3 tầng cho phép bạn chạy toàn bộ logic nghiệp vụ (Pipeline, Tracker, Rule Engine, MQTT, Storage) trên máy tính xách tay cá nhân (Windows hoặc Ubuntu x86) mà không cần board Jetson.

### 1. Cài đặt công cụ cần thiết trên Host PC
* **Ubuntu / Debian**:
  ```bash
  sudo apt-get update
  sudo apt-get install -y build-essential cmake libopencv-dev
  ```
* **Windows**:
  * Cài đặt **Visual Studio 2019/2022** (với C++ Desktop Development) hoặc MinGW-w64.
  * Cài đặt **CMake** (phiên bản $\ge$ 3.16).
  * Cài đặt **OpenCV** (thêm `OpenCV_DIR` vào Environment Variables).

### 2. Biên dịch mã nguồn & Chạy Unit Tests
Mở terminal trong thư mục project:
```bash
# 1. Tạo thư mục build
mkdir build
cd build

# 2. Cấu hình CMake kích hoạt Unit Test
cmake .. -DBUILD_TESTS=ON

# 3. Biên dịch chương trình
cmake --build . --config Release

# 4. Chạy kiểm thử tự động (Unit Tests)
ctest --output-on-failure
```

### 3. Chạy ứng dụng ở chế độ Mock
```bash
# Chạy với cờ --mock (HAL giả lập frame camera và tensor output của GPU)
./edge-ai-manager --config ../config/app_config.yaml --mock
```
*Kết quả trên màn hình:*
```text
[INFO ] Starting Edge AI Object Detection System initialization...
[INFO ] Instantiating MOCK Hardware Abstraction Layer (Host PC Test Mode)...
[INFO ] Loading YOLOv8 Model & TensorRT Engine...
[INFO ] Opening Camera Stream...
[INFO ] Camera streaming started.
[INFO ] Initialization complete. System is ready.
[INFO ] Entering primary detection loop...
```

---

## ⚡ PHẦN 3: Kịch Bản 2 — Test Trực Tiếp Trên Jetson Nano (JetPack 4.6.x)

Trước khi nạp Yocto, nếu bạn đang có sẵn thẻ nhớ chạy JetPack 4.6.x (Ubuntu 18.04), bạn có thể build native để test phần cứng camera thật và GPU thật:

### 1. Chuẩn bị thư viện phụ trợ trên Jetson Nano
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake pkg-config git \
    libsqlite3-dev libyaml-cpp-dev \
    libpaho-mqtt-dev libpaho-mqttpp-dev \
    v4l-utils
```

### 2. Cài đặt Python & Export model YOLOv8 sang ONNX
Chạy trên Host PC hoặc Nano:
```bash
pip3 install ultralytics onnx

# Tải và export YOLOv8 Nano sang ONNX opset 12 (tương thích TensorRT 8.2)
python3 -c "from ultralytics import YOLO; model = YOLO('yolov8n.pt'); model.export(format='onnx', opset=12)"

# Copy file yolov8n.onnx vào thư mục models/
cp yolov8n.onnx /opt/jetson-ai-detect/models/
```

### 3. Cấu hình quạt tản nhiệt & Mode 10W
```bash
# Đặt chế độ 10W tối đa hiệu năng
sudo nvpmodel -m 0
sudo jetson_clocks

# Bật quạt tản nhiệt chạy ở 80% tốc độ
sudo sh -c 'echo 200 > /sys/devices/pwm-fan/target_pwm'
```

### 4. Biên dịch Native trên Jetson Nano
```bash
cd Object-Detection-With-Yolov8-In-Jetson-Nano
mkdir build && cd build

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_JETSON_CUDA=ON \
    -DENABLE_PAHO_MQTT=ON \
    -DENABLE_SQLITE3=ON \
    -DENABLE_YAML_CPP=ON

cmake --build . -j4
```

### 5. Chạy thử nghiệm với Camera CSI & TensorRT thật
```bash
sudo ./edge-ai-manager --config ../config/app_config.yaml
```
*Lần đầu tiên chạy, hệ thống sẽ tự động build file `yolov8n_fp16.engine` từ ONNX (mất ~5-10 phút). Các lần chạy tiếp theo sẽ nạp engine có sẵn chỉ trong 3 giây.*

---

## 📦 PHẦN 4: Kịch Bản 3 — Build Hệ Điều Hành Yocto Hoàn Chỉnh

Đây là mục tiêu chính của dự án: tạo một bản phân phối Linux nhúng chuyên dụng, không có desktop UI nặng nề, khởi động trong 20 giây và tự động chạy `edge-ai-manager`.

### 1. Cấu hình máy Host Build (Ubuntu 20.04 LTS x86_64)
Cài đặt các gói BitBake yêu cầu:
```bash
sudo apt-get update
sudo apt-get install -y \
    gawk wget git diffstat unzip texinfo gcc build-essential \
    chrpath socat cpio python3 python3-pip python3-pexpect \
    xz-utils debianutils iputils-ping python3-git python3-jinja2 \
    libegl1-mesa libsdl1.2-dev pylint3 xterm zstd liblz4-tool
```

### 2. Tải mã nguồn Poky & Các Layer (Nhánh Dunfell 3.1 LTS)
Tạo workspace và clone mã nguồn:
```bash
mkdir -p ~/yocto-jetson && cd ~/yocto-jetson

# 1. Poky Dunfell
git clone -b dunfell git://git.yoctoproject.org/poky

# 2. meta-tegra cho L4T R32.7.x (JetPack 4.6.x)
git clone -b dunfell-l4t-r32.7.x https://github.com/OE4T/meta-tegra.git

# 3. meta-openembedded
git clone -b dunfell https://github.com/openembedded/meta-openembedded.git
```

### 3. Tích hợp Layer của dự án: `meta-jetson-ai-detect`
Copy layer tùy chỉnh trong dự án vào thư mục Yocto:
```bash
cp -r /path/to/Object-Detection-With-Yolov8-In-Jetson-Nano/yocto/meta-jetson-ai-detect ~/yocto-jetson/
```

### 4. Khởi tạo môi trường BitBake
```bash
cd ~/yocto-jetson
source poky/oe-init-build-env build-jetson
```

### 5. Cấu hình `conf/bblayers.conf`
Mở file `conf/bblayers.conf` và thêm các layer sau:
```bitbake
BBLAYERS ?= " \
  ${TOPDIR}/../poky/meta \
  ${TOPDIR}/../poky/meta-poky \
  ${TOPDIR}/../poky/meta-yocto-bsp \
  ${TOPDIR}/../meta-tegra \
  ${TOPDIR}/../meta-openembedded/meta-oe \
  ${TOPDIR}/../meta-openembedded/meta-networking \
  ${TOPDIR}/../meta-openembedded/meta-multimedia \
  ${TOPDIR}/../meta-openembedded/meta-python \
  ${TOPDIR}/../meta-jetson-ai-detect \
"
```

### 6. Cấu hình `conf/local.conf`
Chỉnh sửa file `conf/local.conf`:
```bitbake
# Đặt target machine là Jetson Nano (B01 devkit)
MACHINE = "jetson-nano"

# Chấp nhận giấy phép proprietary của NVIDIA
LICENSE_FLAGS_WHITELIST += "commercial proprietary"

# Tối ưu tải CPU khi build
BB_NUMBER_THREADS = "8"
PARALLEL_MAKE = "-j 8"

# Bật systemd làm init manager chính
INIT_MANAGER = "systemd"

# Bật sstate cache để tăng tốc các lần build tiếp theo
SSTATE_DIR ?= "${TOPDIR}/sstate-cache"
DL_DIR ?= "${TOPDIR}/downloads"
```

### 7. Tiến hành Build Image
```bash
# Build production image (Lần đầu tiên mất khoảng 6 - 12 giờ tùy CPU và mạng)
bitbake jetson-ai-image
```

### 8. Nạp Image vào thẻ nhớ MicroSD
Sau khi BitBake hoàn thành, file image nằm ở:
`tmp/deploy/images/jetson-nano/jetson-ai-image-jetson-nano.tegraflash.zip` hoặc `.sdcard`.

Dùng công cụ `doflash.sh` của meta-tegra hoặc ghi trực tiếp bằng `dd` / `balenaEtcher`:
```bash
# Nếu sinh ra file .sdcard (thay /dev/sdX bằng thẻ nhớ của bạn)
sudo dd if=tmp/deploy/images/jetson-nano/jetson-ai-image-jetson-nano.sdcard of=/dev/sdX bs=4M status=progress conv=fsync
```

---

## 🔍 PHẦN 5: Vận Hành & Khắc Phục Lỗi (Troubleshooting)

Sau khi cắm thẻ nhớ và bật nguồn Jetson Nano:

### 1. Kiểm tra trạng thái Daemon
Hệ thống sẽ tự khởi động service ngầm qua systemd:
```bash
# Kiểm tra trạng thái tiến trình
systemctl status jetson-ai-detect.service

# Xem log thời gian thực qua journald
journalctl -u jetson-ai-detect.service -f
```

### 2. Kiểm tra Camera CSI
Nếu camera không hiển thị hình ảnh, test pipeline GStreamer trực tiếp bằng lệnh:
```bash
gst-launch-1.0 nvarguscamerasrc sensor-id=0 ! 'video/x-raw(memory:NVMM),width=1280,height=720,framerate=30/1' ! nvvidconv ! fakesink
```
*Nếu báo lỗi `nvbuf_utils: dmabuf_fd -1`: Hãy khởi động lại daemon nvargus-daemon:*
```bash
sudo systemctl restart nvargus-daemon
```

### 3. Kiểm tra MQTT Broker kết nối Cloud
* Cài đặt Mosquitto client để bắt bản tin gửi lên broker:
```bash
mosquitto_sub -h 127.0.0.1 -t "edge/jetson-nano-001/detections" -v
```

### 4. Kiểm tra Database SQLite tại chỗ
```bash
sqlite3 /var/lib/jetson-ai-detect/detections.db "SELECT * FROM detections ORDER BY id DESC LIMIT 5;"
```

### 5. Kiểm tra Watchdog phần cứng
```bash
# Kiểm tra thiết bị watchdog có tồn tại không
ls -l /dev/watchdog
```
*HealthService sẽ đều đặn ghi ký tự vào `/dev/watchdog` mỗi 30 giây. Nếu bạn `kill -9` tiến trình `edge-ai-manager` và systemd không restart được, sau 60 giây board Jetson sẽ tự động reboot.*

