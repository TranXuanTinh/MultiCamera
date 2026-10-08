#pragma once

#include "app/types.h"
#include "hal/i_camera_hal.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

namespace edge_ai {

class CameraService {
public:
    explicit CameraService(hal::ICameraHAL& camera_hal, size_t ring_buffer_size = 2);
    ~CameraService();

    bool open(const CameraConfig& config);
    void close();

    // Grabs latest frame from ring buffer (blocking with timeout)
    bool grabFrame(Frame& frame, int timeout_ms = 100);

    float getActualFps() const { return current_fps_; }
    bool isStreaming() const { return running_; }

private:
    void captureLoop();

    hal::ICameraHAL& hal_;
    CameraConfig config_;
    size_t ring_buffer_size_;

    std::atomic<bool> running_{false};
    std::thread capture_thread_;

    std::queue<Frame> buffer_;
    std::mutex mtx_;
    std::condition_variable cv_;

    std::atomic<float> current_fps_{0.0f};
    uint64_t frame_seq_{0};
};

} // namespace edge_ai

