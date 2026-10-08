#pragma once

#include "hal/i_camera_hal.h"
#include <opencv2/imgproc.hpp>
#include <chrono>
#include <thread>

namespace edge_ai { namespace hal {

class MockCameraHAL : public ICameraHAL {
public:
    MockCameraHAL() = default;
    ~MockCameraHAL() override = default;

    bool open(const std::string& /*device*/,
              int width, int height, int fps,
              int /*sensor_id*/ = 0, int /*flip_method*/ = 0) override {
        format_.width = width;
        format_.height = height;
        format_.fps = fps;
        format_.pixel_fmt = PixelFormat::BGR;
        opened_ = true;
        return true;
    }

    bool readFrame(RawFrame& frame) override {
        if (!opened_) return false;

        // Generate synthetic colored frame with counter for testing
        frame.mat = cv::Mat(format_.height, format_.width, CV_8UC3, cv::Scalar(40, 40, 40));
        frame.sequence = ++counter_;

        // Draw a simulated moving object
        int x = static_cast<int>((counter_ * 10) % (format_.width - 100));
        int y = static_cast<int>((counter_ * 5) % (format_.height - 100));
        cv::rectangle(frame.mat, cv::Rect(x, y, 80, 80), cv::Scalar(0, 255, 0), -1);

        // Simulate frame arrival time (e.g. ~30ms for 30fps)
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        return true;
    }

    FrameFormat getFormat() const override { return format_; }
    void release() override { opened_ = false; }
    bool isOpened() const override { return opened_; }

private:
    bool opened_{false};
    FrameFormat format_;
    uint64_t counter_{0};
};

}} // namespace edge_ai::hal

