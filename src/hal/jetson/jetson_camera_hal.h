#pragma once

#include "hal/i_camera_hal.h"
#include <opencv2/videoio.hpp>
#include <string>

namespace edge_ai { namespace hal {

class JetsonCameraHAL : public ICameraHAL {
public:
    JetsonCameraHAL();
    ~JetsonCameraHAL() override;

    bool open(const std::string& device,
              int width, int height, int fps,
              int sensor_id = 0, int flip_method = 0) override;

    bool readFrame(RawFrame& frame) override;
    FrameFormat getFormat() const override;
    void release() override;
    bool isOpened() const override;

private:
    std::string buildGstreamerPipeline(int sensor_id, int width, int height, int fps, int flip_method);

    cv::VideoCapture cap_;
    FrameFormat format_;
    uint64_t frame_seq_{0};
};

}} // namespace edge_ai::hal

