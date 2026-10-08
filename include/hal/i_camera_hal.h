#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <opencv2/core.hpp>

namespace edge_ai { namespace hal {

enum class PixelFormat {
    BGR,
    RGB,
    NV12,
    YUYV
};

struct FrameFormat {
    int width{0};
    int height{0};
    int fps{0};
    PixelFormat pixel_fmt{PixelFormat::BGR};
};

struct RawFrame {
    cv::Mat mat;           // Wrapped or converted image matrix
    uint64_t sequence{0};  // Hardware/driver monotonic frame counter
};

class ICameraHAL {
public:
    virtual ~ICameraHAL() = default;

    virtual bool open(const std::string& device,
                      int width, int height, int fps,
                      int sensor_id = 0, int flip_method = 0) = 0;
    virtual bool readFrame(RawFrame& frame) = 0;
    virtual FrameFormat getFormat() const = 0;
    virtual void release() = 0;
    virtual bool isOpened() const = 0;
};

}} // namespace edge_ai::hal

