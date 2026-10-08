#include "service/camera_service.h"
#include <chrono>

namespace edge_ai {

CameraService::CameraService(hal::ICameraHAL& camera_hal, size_t ring_buffer_size)
    : hal_(camera_hal), ring_buffer_size_(ring_buffer_size) {
}

CameraService::~CameraService() {
    close();
}

bool CameraService::open(const CameraConfig& config) {
    close();
    config_ = config;

    bool ok = hal_.open(config.device, config.width, config.height, config.fps,
                        config.csi_sensor_id, config.csi_flip_method);
    if (!ok) return false;

    running_ = true;
    capture_thread_ = std::thread(&CameraService::captureLoop, this);
    return true;
}

void CameraService::close() {
    running_ = false;
    cv_.notify_all();

    if (capture_thread_.joinable()) {
        capture_thread_.join();
    }
    hal_.release();

    std::lock_guard<std::mutex> lock(mtx_);
    while (!buffer_.empty()) buffer_.pop();
}

void CameraService::captureLoop() {
    auto last_fps_time = std::chrono::steady_clock::now();
    int fps_counter = 0;

    while (running_) {
        hal::RawFrame raw;
        if (!hal_.readFrame(raw) || raw.mat.empty()) {
            // Reconnect attempt on capture failure
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (!hal_.isOpened()) {
                hal_.open(config_.device, config_.width, config_.height, config_.fps,
                          config_.csi_sensor_id, config_.csi_flip_method);
            }
            continue;
        }

        Frame f;
        f.image = raw.mat; // cv::Mat shallow copy (ref counted)
        f.frame_id = ++frame_seq_;
        f.timestamp = std::chrono::steady_clock::now();
        f.width = raw.mat.cols;
        f.height = raw.mat.rows;

        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (buffer_.size() >= ring_buffer_size_) {
                // Drop oldest frame to ensure zero accumulated latency
                buffer_.pop();
            }
            buffer_.push(std::move(f));
        }
        cv_.notify_one();

        // Calculate moving average FPS
        ++fps_counter;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<float>(now - last_fps_time).count();
        if (elapsed >= 1.0f) {
            current_fps_ = static_cast<float>(fps_counter) / elapsed;
            fps_counter = 0;
            last_fps_time = now;
        }
    }
}

bool CameraService::grabFrame(Frame& frame, int timeout_ms) {
    std::unique_lock<std::mutex> lock(mtx_);
    bool available = cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                  [this]() { return !buffer_.empty() || !running_; });

    if (!available || buffer_.empty()) {
        return false;
    }

    frame = std::move(buffer_.front());
    buffer_.pop();
    return true;
}

} // namespace edge_ai

