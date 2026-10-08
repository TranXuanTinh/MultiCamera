#pragma once

#include "app/types.h"
#include "service/camera_service.h"
#include "service/inference_service.h"
#include "service/tracker_service.h"
#include <chrono>

namespace edge_ai {

class Pipeline {
public:
    Pipeline(CameraService& camera,
             InferenceService& inference,
             TrackerService& tracker);
    ~Pipeline() = default;

    bool start();
    void stop();

    // Executes one synchronous cycle of: Capture -> Infer -> Track
    bool processOneFrame(PipelineResult& result);

    float getCurrentFps() const { return current_fps_; }

private:
    CameraService& camera_;
    InferenceService& inference_;
    TrackerService& tracker_;

    bool running_{false};
    uint64_t frame_count_{0};

    float current_fps_{0.0f};
    std::chrono::steady_clock::time_point fps_timer_;
    int fps_counter_{0};
};

} // namespace edge_ai

