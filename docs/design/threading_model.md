# Tài Liệu Thiết Kế Đa Luồng & Quản Trị Tài Nguyên (Threading & Resource Model)
## Dự Án: Hệ Thống Multi-Camera AI Trên NVIDIA Jetson Bằng DeepStream SDK & C++17

Tài liệu này mô tả chi tiết kiến trúc điều phối tiến trình và đa luồng (Process & Multi-threading Architecture), cơ chế ánh xạ phần cứng Jetson Nano, mô hình quản trị tài nguyên hai tầng (Two-Tier Resource Governance), và thứ tự giải phóng luồng an toàn (Graceful Shutdown) của hệ thống đa camera.

---

## 1. Bản Đồ Ánh Xạ Phần Cứng & Luồng Xử Lý Trên Jetson Nano

Khác với kiến trúc x86 truyền thống, SoC NVIDIA Tegra X1 (Jetson Nano) sở hữu cấu trúc tính toán dị thể (Heterogeneous Computing) tích hợp CPU, GPU và các bộ tăng tốc phần cứng chuyên dụng trên cùng một vùng nhớ chia sẻ **Unified Memory (4GB LPDDR4)**:

```
Jetson Nano 4GB
┌──────────────────────────────────────────────────────────────────────────┐
│ CPU (Quad-core Cortex-A57 @ 1.43 GHz)                                   │
│                                                                          │
│ Main/GLib Loop        Source Ingestion Tasks     Business Logic Workers  │
│ ┌────────────────┐   ┌────────────────────────┐ ┌──────────────────────┐ │
│ │• Lifecycle Mgmt│   │• CSI driver control    │ │• RuleEngine          │ │
│ │• GstBus Watch  │   │• RTSP/RTP Demux (x N)  │ │• MqttService (Async) │ │
│ │• POSIX Signals │   │• Metadata Map (src->id)│ │• SQLite / Health     │ │
│ └────────────────┘   └────────────────────────┘ └──────────────────────┘ │
├──────────────────────────────────────────────────────────────────────────┤
│ Hardware Accelerators (Fixed-Function ASICs - Không chiếm CPU/GPU)       │
│                                                                          │
│ CSI Sensor ──► Tegra ISP       RTSP Bitstream ──► NVDEC   Scaler ──► VIC │
│ (Demosaic, AWB, AE)            (H.264/H.265 Decode)       (Aspect Letter)│
├──────────────────────────────────────────────────────────────────────────┤
│ GPU (128-core Maxwell @ 921 MHz)                                         │
│                                                                          │
│ nvstreammux Batched Memory Surface                                       │
│         │                                                                │
│         ▼                                                                │
│ TensorRT YOLO Batched Primary GIE (FP16 / INT8)                          │
│         │                                                                │
│         ▼                                                                │
│ nvtracker (Multi-Stream Tracking Engine - NvDCF / NvSORT)                │
├──────────────────────────────────────────────────────────────────────────┤
│ Unified RAM (4GB LPDDR4 @ 1600MHz - 25.6 GB/s)                           │
│                                                                          │
│ Shared System: OS, TensorRT Engine Weights, CUDA Context, Muxer Pool    │
│                                                                          │
│ Cam 0: CSI capture NVMM buffers + queue + source/rule state              │
│ Cam 1: RTSP decode NVMM buffers + jitter buffer + queue + rule state     │
│ Cam 2: RTSP decode NVMM buffers + jitter buffer + queue + rule state     │
│ Cam N: Ingestion buffers + jitter buffer + queue + rule state            │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Bảng Đặc Tả Chi Tiết Các Luồng Trong Hệ Thống

| Nhóm Luồng | Tên Luồng | Tầng kiến trúc | Quản lý bởi | Chu kỳ / Tần số | Mức chiếm CPU | Vai trò kỹ thuật chính |
|---|---|---|---|---|---|---|
| **App** | **Main Thread** | App Layer | `main.cpp`<br>`EdgeAIManager::run()` | Định kỳ 50ms | < 1% | Giám sát trạng thái, bắt tín hiệu dừng `SIGINT`/`SIGTERM` |
| **DeepStream** | **Source Ingestion (1..N)** | GStreamer HAL | `nvurisrcbin` / `nvarguscamerasrc` | 30 FPS / camera | Tải nhẹ (I/O) | Nhận gói tin RTP RTSP hoặc tín hiệu MIPI CSI, chuyển sang phần cứng NVDEC/ISP |
| **DeepStream** | **StreamMux Collector** | GStreamer HAL | `nvstreammux` | 25 – 30 batches/s | 3% – 5% | Gom $N$ frame thành 1 batched surface, kích hoạt timeout đồng bộ 40ms |
| **DeepStream** | **PGIE & Tracker Worker** | GStreamer / CUDA | `nvinfer` & `nvtracker` | 25 – 30 batches/s | < 5% CPU<br>**(GPU Execute)** | Nạp batch vào GPU TensorRT và thực thi bám vết đa camera NvDCF |
| **DeepStream** | **Pad Probe Thread** | GStreamer HAL | `JetsonDeepStreamHAL::padProbeCallback` | Bất đồng bộ theo batch | 2% – 3% | Duyệt metadata `NvDsBatchMeta` zero-copy, đóng gói `PipelineResult` |
| **Service** | **EventBus Dispatcher** | Service Layer | `EventBus::dispatchLoop()` | Bất đồng bộ | < 1% | Hàng đợi bounded queue chuyển giao metadata sang MQTT và SQLite |
| **Service** | **Storage Writer** | Service Layer | `StorageService::writerLoop()` | 5 giây / lần | < 1% | Ghi transaction mẻ vào bảng SQLite có index `camera_id` |
| **Service** | **MQTT Worker** | Service / HAL | Paho MQTT C runtime | Bất đồng bộ | 1% – 2% | Quản lý socket TCP/TLS, publish telemetry theo từng `camera_id` |
| **Service** | **Health & Watchdog** | Service Layer | `HealthService::monitorLoop()` | 30 giây / lần | < 0.5% | Đọc sysfs nhiệt độ, đá `/dev/watchdog` duy trì hệ thống |

### 2.1 Luồng Trong Chế Độ Modular Fallback Pipeline (Khi không chạy DeepStream)
Khi ứng dụng chạy ở chế độ đơn kênh / kiểm tra độc lập dịch vụ:
* **`CameraService`**: Quản lý luồng thu nhận ảnh tuần tự từ `ICameraHAL` (`grabFrame()`), duy trì tốc độ 30 FPS.
* **`InferenceService`**: Thực thi tiền xử lý (`Preprocessor`), chuyển giao buffer sang `IGpuHAL::runInference()` trên GPU Stream, và giải mã tensor (`Postprocessor`).
* **`TrackerService`**: Cập nhật ma trận hiệp phương sai `KalmanFilter` và giải bài toán gán cặp `Hungarian` trên CPU trong thời gian $< 2\text{ ms}$ cho mỗi frame.

---

## 3. Triết Lý Quản Trị Tài Nguyên Hai Tầng (Two-Tier Resource Governance)

> [!IMPORTANT]
> **Không phân bổ cứng CPU/GPU theo tỷ lệ phần trăm tĩnh cho từng camera.**
> Trên kiến trúc SoC Tegra chia sẻ Unified Memory và chạy batched pipeline (`nvstreammux` + `nvinfer`):
> 1. GPU TensorRT chạy suy luận trên toàn bộ mẻ tensor $[B, 3, 640, 640]$ cùng một lúc, không thể chia cắt tĩnh 25% cho Cam 1 và 25% cho Cam 2.
> 2. Các bộ tăng tốc phần cứng (Tegra ISP, NVDEC, VIC) hoạt động độc lập bằng vi mạch ASIC chuyên dụng, hoàn toàn không tiêu tốn chu kỳ lệnh CPU/GPU.
> 3. Tải CPU đến từ thông lượng mạng (RTP packets) và tần suất phát sinh sự kiện logic (Rule violations), biến thiên theo ngữ cảnh thực tế.

Do đó, hệ thống áp dụng mô hình **Quản trị tài nguyên 2 tầng**:

```
                              HAI TẦNG QUẢN TRỊ TÀI NGUYÊN
    ┌────────────────────────────────────────────────────────────────────────┐
    │ 1. PIPELINE-LEVEL METRICS (Quản trị toàn cục hệ thống)                 │
    │    • Tổng tải GPU (%) qua tegrastats / NVML                            │
    │    • Tổng Unified RAM chiếm dụng (OS + TensorRT Weights + Buffer Pools)│
    │    • Tần số xử lý mẻ toàn hệ thống (Pipeline Batch FPS)                │
    │    • Độ trễ xử lý trọn vòng (End-to-end Pipeline Latency ms)           │
    └───────────────────────────────────┬────────────────────────────────────┘
                                        │ phân rã theo dõi
    ┌───────────────────────────────────▼────────────────────────────────────┐
    │ 2. PER-CAMERA RUNTIME METRICS (Quản trị độc lập theo từng cameraId)    │
    │    • input_fps           : Tần số khung hình thực tế nhận từ nguồn     │
    │    • processed_fps       : Tần số khung hình thực tế qua Pad Probe     │
    │    • dropped_frames      : Khung hình bị rơi rụng (nghẽn mạng/jitter)  │
    │    • last_pts            : Timestamp mới nhất (phát hiện cam đóng băng)│
    │    • decode_latency_ms   : Độ trễ giải mã phần cứng NVDEC/ISP          │
    │    • result_queue_depth  : Độ sâu hàng đợi bounded queue của kênh      │
    │    • active_objects      : Số lượng vật thể đang được track đồng thời  │
    │    • per-camera state    : Jitter buffer, ring buffer, bộ đếm rule     │
    └────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Thứ Tự Dừng Luồng An Toàn (Graceful Shutdown Order)

Để ngăn ngừa triệt để lỗi truy cập vùng nhớ rác (Segmentation Fault), rò rỉ buffer NVMM DMA, hoặc mất mát dữ liệu SQLite chưa kịp ghi đĩa, hệ thống thực thi thứ tự dừng an toàn từ **Đầu vào (Ingress) $\to$ Đầu ra (Egress) $\to$ Xử lý nền (Workers)**:

```
[Bước 1: DỪNG NGUỒN STREAMING]
         │  • pipeline_->stop() gửi tín hiệu dừng tới GStreamer
         │  • Ngắt nhận gói tin từ CSI / RTSP IP camera
         ▼  • Chuyển trạng thái Pipeline sang GST_STATE_NULL
[Bước 2: THÁO GỠ PAD PROBE]
         │  • deepstream_hal_->stop() ngắt Probe Callback
         │  • Đảm bảo không còn metadata callback nào được bắn vào App
         ▼
[Bước 3: DRAIN HÀNG ĐỢI BOUNDED QUEUE]
         │  • event_bus_->stop() dừng nhận sự kiện mới
         │  • Luồng Dispatcher xử lý nốt toàn bộ sự kiện in-flight còn lại
         ▼  • Chờ hàng đợi rỗng hoàn toàn và join() dispatch_thread_
[Bước 4: FLUSH & ĐÓNG DỮ LIỆU SQLITE]
         │  • storage_svc_->shutdown() thực thi Commit transaction cuối cùng
         ▼  • Đóng file cơ sở dữ liệu SQLite sạch sẽ, flush cache đĩa
[Bước 5: NGẮT KẾT NỐI MQTT TELEMETRY]
         │  • mqtt_svc_->disconnect() gửi gói tin MQTT DISCONNECT
         ▼  • Đóng socket TCP/TLS, giải phóng bộ đệm mạng
[Bước 6: AN TOÀN WATCHDOG & GIẢI PHÓNG TÀI NGUYÊN]
            • health_svc_->stop() ghi ký tự 'V' vào /dev/watchdog
            • Đóng file descriptor watchdog tránh kích hoạt reboot khẩn cấp
            • Thu hồi toàn bộ GPU memory context và thoát tiến trình sạch sẽ
```

---

## 5. Cơ Chế Bounded Queue & Chống Tràn Bộ Nhớ (Backpressure)

* **Bounded Queue trong `EventBus`**: Hàng đợi ring buffer dung lượng tối đa $256$ sự kiện. Nếu I/O đĩa hoặc mạng bị trễ tạm thời, hàng đợi tự động drop bản ghi cũ nhất theo chính sách FIFO ring-buffer để bảo toàn Unified RAM.
* **Tách biệt luồng xử lý**: Việc tính toán logic (`RuleEngine`), ghi đĩa SQLite (`StorageService`), và truyền tin mạng (`MqttService`) nằm hoàn toàn trên các luồng worker độc lập, đảm bảo Pad Probe callback luôn hoàn thành dưới $0.05\text{ ms}$, không bao giờ làm trễ luồng DeepStream GStreamer.
