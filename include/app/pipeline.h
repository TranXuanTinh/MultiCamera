#pragma once

#include "app/types.h"
#include "service/camera_service.h"
#include "service/inference_service.h"
#include "service/tracker_service.h"
#include "hal/i_deepstream_hal.h"
#include <chrono>
#include <functional>
#include <mutex>

namespace edge_ai {

class Pipeline {
public:
    // Multi-Camera DeepStream constructor
    explicit Pipeline(hal::IDeepStreamHAL& deepstream_hal);

    // Legacy Single-Camera constructor (for backwards compatibility)
    Pipeline(CameraService& camera,
             InferenceService& inference,
             TrackerService& tracker);
    ~Pipeline();

    bool start();
    void stop();

    using ResultCallback = std::function<void(const PipelineResult&)>;
    void setResultCallback(ResultCallback cb);

    // Executes one synchronous cycle or steps one frame
    bool processOneFrame(PipelineResult& result);

    float getCurrentFps() const { return current_fps_; }
    bool isDeepStream() const { return deepstream_mode_; }

private:
    void onDeepStreamMetadata(const PipelineResult& result);

    hal::IDeepStreamHAL* ds_hal_{nullptr};
    CameraService* camera_{nullptr};
    InferenceService* inference_{nullptr};
    TrackerService* tracker_{nullptr};

    bool deepstream_mode_{false};
    bool running_{false};
    uint64_t frame_count_{0};

    float current_fps_{0.0f};
    std::chrono::steady_clock::time_point fps_timer_;
    int fps_counter_{0};

    ResultCallback result_callback_;
    std::mutex cb_mtx_;
};

} // namespace edge_ai

