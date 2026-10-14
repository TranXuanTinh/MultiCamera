# 📖 Hướng Dẫn Cài Đặt & Triển Khai (Setup Guide)
## Dự Án: Multi-Camera Video Analytics Framework Dựa Trên NVIDIA DeepStream & Yocto Linux (C++17)

Tài liệu này hướng dẫn chi tiết từng bước cách thiết lập phần cứng đa camera, cài đặt môi trường, biên dịch và triển khai hệ thống theo 3 kịch bản:
1. **Kịch bản 1**: Chạy thử nghiệm và Unit Test đa camera trên **Host PC (Linux x86 / Windows)** với **MockDeepStreamHAL** (không cần board Jetson hay DeepStream SDK).
2. **Kịch bản 2**: Biên dịch và chạy native trên **NVIDIA Jetson (JetPack 4.6.x / JetPack 5.x)** với **NVIDIA DeepStream SDK** tăng tốc phần cứng (NVDEC, nvstreammux, nvinfer, nvtracker).
3. **Kịch bản 3**: Build hệ điều hành nhúng hoàn chỉnh bằng **Yocto Project** tích hợp layer `meta-jetson-ai-detect` và nạp vào thẻ nhớ MicroSD.

---

## 🔌 PHẦN 1: Thiết Lập Phần Cứng Đa Camera (Hardware Setup)

Hệ thống hỗ trợ chạy kết hợp nhiều nguồn camera đồng thời (CSI nội bộ và IP Camera RTSP qua mạng LAN):

```
                   +--------------------------------------------+
                   |            Jetson Developer Kit            |
                   |                                            |
[Nguồn 5V/4A DC] ->| J25 (Barrel Jack)                          |
[Jumper J48]    -->| Nối tắt J48 (Disable Micro-USB)            |
[Camera CSI 1]  -->| J13 (CAM0 - IMX219)                        |
[Camera CSI 2]  -->| J49 (CAM1 - IMX219)                        |
[Quạt 5V PWM]   -->| J15 (4-pin PWM Fan Header)                 |
[PoE Switch LAN]-->| RJ45 Gigabit Ethernet                      |
                   +---------------------+----------------------+
                                         |
                       +-----------------+-----------------+
                       |                                   |
              [IP Camera 1 (RTSP)]                [IP Camera 2 (RTSP)]
              192.168.1.101:554/ch0               192.168.1.102:554/ch0
```

### 1. Cấu hình Nguồn 5V/4A Barrel Jack
* **Bắt buộc**: Dùng nguồn ngoài qua cổng Barrel Jack **5V/4A DC (J25)**.
* **Cắm Jumper J48**: Tìm header 2 chân J48 (nằm ngay dưới cổng Camera CAM0). **Nối tắt (short) 2 chân này bằng jumper 2.54mm** để ngắt nguồn cổng Micro-USB và kích hoạt cổng Barrel Jack.
* *Lưu ý*: Xử lý đa luồng camera (Multi-Stream NVDEC + TensorRT FP16) tiêu thụ từ 8W - 12W. Nguồn Micro-USB 5V/2A sẽ bị sụt áp tức thì dẫn đến treo hoặc reboot phần cứng (Brownout Reset).

### 2. Kết nối Cặp Camera MIPI CSI-2 (Dual CSI Sony IMX219)
* **CAM0 (J13)**: Camera kênh 0 (`sensor-id=0`). Cắm cáp ribbon với mặt tiếp xúc màu bạc hướng về phía tản nhiệt (SoC).
* **CAM1 (J49)**: Camera kênh 1 (`sensor-id=1`). Cắm cáp ribbon với mặt tiếp xúc màu bạc hướng về phía tản nhiệt.
* Cố định chốt nhựa chắc chắn để tránh nhiễu tín hiệu MIPI CSI-2 tốc độ cao.

### 3. Kết nối IP Camera RTSP qua Mạng LAN
* Cắm cổng Gigabit Ethernet của Jetson vào Switch mạng (khuyến nghị PoE Gigabit Switch).
* Đặt địa chỉ IP tĩnh cho Jetson và các Camera IP trong cùng dải mạng con (ví dụ: `192.168.1.0/24`).
* Kiểm tra luồng RTSP của từng camera bằng VLC hoặc lệnh `ffplay rtsp://admin:pass@192.168.1.101:554/stream1`.

### 4. Kết nối Quạt tản nhiệt PWM (J15)
* Cắm quạt 4-pin 5V vào header J15. Quạt sẽ được điều khiển tự động bởi `HealthService` qua PWM kernel để giải nhiệt cho GPU và bộ giải mã NVDEC.

---

## 💻 PHẦN 2: Kịch Bản 1 — Test Đa Camera Trên Host PC Với Mock HAL

Kiến trúc HAL (`IDeepStreamHAL`) tách biệt hoàn toàn giữa ứng dụng và driver phần cứng. Trên máy tính xách tay cá nhân (Ubuntu x86 hoặc Windows), bạn có thể chạy mô phỏng $N$ luồng camera, kiểm thử thuật toán tracking và quy tắc phân tích mà không cần Jetson.

### 1. Cài đặt môi trường Host PC
* **Ubuntu / Debian**:
  ```bash
  sudo apt-get update
  sudo apt-get install -y build-essential cmake pkg-config libopencv-dev
  ```

### 2. Biên dịch mã nguồn & Chạy toàn bộ Unit Tests
Mở terminal tại thư mục gốc của project:
```bash
# 1. Tạo thư mục build
mkdir -p build && cd build

# 2. Cấu hình CMake kích hoạt Unit Tests (Mặc định ENABLE_DEEPSTREAM=OFF trên x86)
cmake .. -DBUILD_TESTS=ON

# 3. Biên dịch chương trình
cmake --build . --config Release -j$(nproc)

# 4. Chạy kiểm thử tự động (Unit Tests cho Pipeline đa luồng và RuleEngine per-camera)
ctest --output-on-failure
```
*Kết quả kiểm thử mẫu:*
```text
1/3 Test #1: test_pipeline ....................   Passed    0.12 sec
2/3 Test #2: test_tracker .....................   Passed    0.05 sec
3/3 Test #3: test_rule_engine .................   Passed    0.08 sec

100% tests passed, 0 tests failed out of 3
```

### 3. Chạy Ứng Dụng Ở Chế Độ Mock Đa Camera
```bash
# Khởi động ứng dụng với cờ --mock
./edge-ai-manager --config ../config/app_config.yaml --mock
```
*Giao diện log trên màn hình:*
```text
[INFO] Loading application configuration: ../config/app_config.yaml
[INFO] DeepStream Multi-Camera mode enabled. Channels: 2, Batch size: 2
[INFO] Channel 0: ID=cam_01, Type=csi, Source=0
[INFO] Channel 1: ID=cam_02, Type=rtsp, Source=rtsp://192.168.1.101:554/stream1
[INFO] Instantiating MOCK DeepStream HAL (Host Multi-Stream Simulation)...
[INFO] Mock DeepStream HAL initialized with 2 camera stream channels.
[INFO] Rule Engine loaded 2 active rules from ../config/rules.yaml
[INFO] DeepStream batch callback registered. Starting pipeline...
[INFO] [cam_01] PipelineResult received: Batch size 1, Detections: 2
[INFO] [cam_02] PipelineResult received: Batch size 1, Detections: 1
```

---

## ⚡ PHẦN 3: Kịch Bản 2 — Chạy Native Trên Jetson Với DeepStream Thật

Khi triển khai trực tiếp trên board Jetson Nano hoặc Jetson Orin Nano đã cài sẵn NVIDIA JetPack (4.6.x hoặc 5.x) và DeepStream SDK:

### 1. Cài đặt các gói phụ trợ
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake pkg-config git \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    libsqlite3-dev libyaml-cpp-dev \
    libpaho-mqtt-dev libpaho-mqttpp-dev \
    v4l-utils
```

### 2. Chuẩn bị File Cấu Hình DeepStream (PGIE & Tracker)
Trong thư mục `config/` đã có sẵn các file cấu hình chuẩn:
* `config/deepstream_pgie_config.txt`: Cấu hình PGIE nvinfer (YOLOv8 INT8/FP16, batch-size = 2 hoặc 4, pad probe buffer).
* `config/deepstream_tracker_config.txt`: Cấu hình thuật toán NVTracker (IOU / NvSORT tracker với tracking per-stream).
* `config/app_config.yaml`: Danh sách camera nguồn:
  ```yaml
  cameras:
    - id: "cam_01"
      type: "csi"
      source: "0"
      width: 1280
      height: 720
      fps: 30
      enabled: true

    - id: "cam_02"
      type: "rtsp"
      source: "rtsp://192.168.1.101:554/live/ch0"
      width: 1280
      height: 720
      fps: 25
      enabled: true

  deepstream:
    enabled: true
    batch_size: 2
    batched_push_timeout: 40000 # 40ms timeout for nvstreammux
    pgie_config_path: "/opt/jetson-ai-detect/config/deepstream_pgie_config.txt"
    tracker_config_path: "/opt/jetson-ai-detect/config/deepstream_tracker_config.txt"
  ```

### 3. Tối ưu Xung Nhịp & Quạt Tản Nhiệt
```bash
# Thiết lập chế độ 10W MAXN (Jetson Nano)
sudo nvpmodel -m 0
sudo jetson_clocks

# Bật quạt tản nhiệt chạy ở 80% công suất
sudo sh -c 'echo 200 > /sys/devices/pwm-fan/target_pwm'
```

### 4. Biên dịch Dự Án Với Tùy Chọn DeepStream
```bash
cd /path/to/MultiCamera
mkdir -p build && cd build

# Kích hoạt flag ENABLE_DEEPSTREAM để liên kết thư viện DeepStream SDK
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_DEEPSTREAM=ON \
    -DENABLE_PAHO_MQTT=ON \
    -DENABLE_SQLITE3=ON \
    -DENABLE_YAML_CPP=ON

# Biên dịch song song trên 4 nhân ARM
cmake --build . -j4
```

### 5. Chạy Thử Nghiệm Native
```bash
sudo ./edge-ai-manager --config ../config/app_config.yaml
```
*Khi chạy lần đầu, `nvinfer` sẽ tự động biên dịch TensorRT Engine với `batch-size=2` tương ứng từ model ONNX/weights. Quá trình này mất khoảng 5–8 phút và lưu file cache `.engine` để nạp ngay tức thì trong những lần chạy sau.*

---

## 📦 PHẦN 4: Kịch Bản 3 — Build Image Yocto Linux Hoàn Chỉnh

Xây dựng bản phân phối Linux nhúng chuyên dụng, không desktop X11, thời gian khởi động < 20 giây và tự động chạy daemon `jetson-ai-detect`.

### 1. Chuẩn bị máy Host Build (Ubuntu 20.04 LTS x86_64)
```bash
sudo apt-get update
sudo apt-get install -y \
    gawk wget git diffstat unzip texinfo gcc build-essential \
    chrpath socat cpio python3 python3-pip python3-pexpect \
    xz-utils debianutils iputils-ping python3-git python3-jinja2 \
    libegl1-mesa libsdl1.2-dev pylint3 xterm zstd liblz4-tool
```

### 2. Tải Poky và các Layer Yocto (Nhánh Dunfell 3.1 LTS)
```bash
mkdir -p ~/yocto-jetson && cd ~/yocto-jetson
git clone -b dunfell git://git.yoctoproject.org/poky
git clone -b dunfell-l4t-r32.7.x https://github.com/OE4T/meta-tegra.git
git clone -b dunfell https://github.com/openembedded/meta-openembedded.git
```

### 3. Tích hợp Layer `meta-jetson-ai-detect`
```bash
cp -r /path/to/MultiCamera/yocto/meta-jetson-ai-detect ~/yocto-jetson/
```

### 4. Khởi tạo môi trường BitBake
```bash
cd ~/yocto-jetson
source poky/oe-init-build-env build-jetson
```

### 5. Cấu hình `conf/bblayers.conf`
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
```bitbake
MACHINE = "jetson-nano"
LICENSE_FLAGS_WHITELIST += "commercial proprietary"
BB_NUMBER_THREADS = "8"
PARALLEL_MAKE = "-j 8"
INIT_MANAGER = "systemd"
SSTATE_DIR ?= "${TOPDIR}/sstate-cache"
DL_DIR ?= "${TOPDIR}/downloads"
```

### 7. Tiến hành Build & Ghi Thẻ Nhớ
```bash
# Build hệ điều hành nhúng
bitbake jetson-ai-image

# Ghi file image vào thẻ nhớ MicroSD (ví dụ /dev/sdX)
sudo dd if=tmp/deploy/images/jetson-nano/jetson-ai-image-jetson-nano.sdcard of=/dev/sdX bs=4M status=progress conv=fsync
```

---

## 🔍 PHẦN 5: Vận Hành & Xử Lý Sự Cố Đa Luồng Camera (Troubleshooting)

### 1. Kiểm tra trạng thái Daemon Systemd
```bash
systemctl status jetson-ai-detect.service
journalctl -u jetson-ai-detect.service -f
```

### 2. Kiểm tra Test Độc Lập Các Luồng Camera
* **Kiểm tra Camera CSI 0 & 1:**
  ```bash
  gst-launch-1.0 nvarguscamerasrc sensor-id=0 ! 'video/x-raw(memory:NVMM),width=1280,height=720,framerate=30/1' ! nvvidconv ! fakesink
  gst-launch-1.0 nvarguscamerasrc sensor-id=1 ! 'video/x-raw(memory:NVMM),width=1280,height=720,framerate=30/1' ! nvvidconv ! fakesink
  ```
  *Nếu gặp lỗi `nvbuf_utils: dmabuf_fd -1` hoặc daemon bị treo, khởi động lại:*
  ```bash
  sudo systemctl restart nvargus-daemon
  ```

* **Kiểm tra luồng RTSP qua bộ giải mã phần cứng NVDEC:**
  ```bash
  gst-launch-1.0 rtspsrc location=rtsp://192.168.1.101:554/stream1 latency=100 ! rtph264depay ! h264parse ! nvv4l2decoder ! nvvidconv ! fakesink
  ```

### 3. Kiểm tra Bản Tin MQTT Theo Từng Kênh Camera
Bắt bản tin theo từng camera hoặc wildcard:
```bash
# Lắng nghe sự kiện từ tất cả các camera
mosquitto_sub -h 127.0.0.1 -t "edge/jetson-nano-001/+/detections" -v

# Lắng nghe cảnh báo vi phạm vùng/đếm người từ camera cụ thể
mosquitto_sub -h 127.0.0.1 -t "edge/jetson-nano-001/cam_01/alerts" -v
```

### 4. Kiểm tra Dữ Liệu SQLite Lưu Trữ
Kiểm tra bản ghi phát hiện vật thể theo từng kênh camera:
```bash
sqlite3 /var/lib/jetson-ai-detect/detections.db \
  "SELECT camera_id, class_name, confidence, timestamp FROM detections ORDER BY id DESC LIMIT 10;"
```
