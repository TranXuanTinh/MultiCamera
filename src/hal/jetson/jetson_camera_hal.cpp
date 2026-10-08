#include "jetson_camera_hal.h"
#include <sstream>

namespace edge_ai { namespace hal {

JetsonCameraHAL::JetsonCameraHAL() = default;

JetsonCameraHAL::~JetsonCameraHAL() {
    release();
}

std::string JetsonCameraHAL::buildGstreamerPipeline(int sensor_id, int width, int height, int fps, int flip_method) {
    std::ostringstream ss;
    ss << "nvarguscamerasrc sensor-id=" << sensor_id << " ! "
       << "video/x-raw(memory:NVMM), width=(int)" << width << ", height=(int)" << height
       << ", format=(string)NV12, framerate=(fraction)" << fps << "/1 ! "
       << "nvvidconv flip-method=" << flip_method << " ! "
       << "video/x-raw, format=(string)BGRx ! "
       << "videoconvert ! "
       << "video/x-raw, format=(string)BGR ! "
       << "appsink drop=true max-buffers=1";
    return ss.str();
}

bool JetsonCameraHAL::open(const std::string& device,
                           int width, int height, int fps,
                           int sensor_id, int flip_method) {
    release();

    format_.width = width;
    format_.height = height;
    format_.fps = fps;
    format_.pixel_fmt = PixelFormat::BGR;

    if (device == "csi" || device == "/dev/video0_csi" || sensor_id >= 0) {
        std::string gst = buildGstreamerPipeline(sensor_id, width, height, fps, flip_method);
        cap_.open(gst, cv::CAP_GSTREAMER);
    }

    if (!cap_.isOpened()) {
        // Fallback to standard V4L2 USB camera
        cap_.open(device, cv::CAP_V4L2);
        if (cap_.isOpened()) {
            cap_.set(cv::CAP_PROP_FRAME_WIDTH, width);
            cap_.set(cv::CAP_PROP_FRAME_HEIGHT, height);
            cap_.set(cv::CAP_PROP_FPS, fps);
        }
    }

    return cap_.isOpened();
}

bool JetsonCameraHAL::readFrame(RawFrame& frame) {
    if (!cap_.isOpened()) return false;

    if (!cap_.read(frame.mat) || frame.mat.empty()) {
        return false;
    }

    frame.sequence = ++frame_seq_;
    return true;
}

FrameFormat JetsonCameraHAL::getFormat() const {
    return format_;
}

void JetsonCameraHAL::release() {
    if (cap_.isOpened()) {
        cap_.release();
    }
}

bool JetsonCameraHAL::isOpened() const {
    return cap_.isOpened();
}

}} // namespace edge_ai::hal

