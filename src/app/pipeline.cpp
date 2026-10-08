#include "app/pipeline.h"

namespace edge_ai {

Pipeline::Pipeline(CameraService& camera,
                   InferenceService& inference,
                   TrackerService& tracker)
    : camera_(camera), inference_(inference), tracker_(tracker) {
}

bool Pipeline::start() {
    running_ = true;
    frame_count_ = 0;
    fps_timer_ = std::chrono::steady_clock::now();
    fps_counter_ = 0;
    return true;
}

void Pipeline::stop() {
    running_ = false;
}

bool Pipeline::processOneFrame(PipelineResult& result) {
    if (!running_) return false;

    // 1. Capture Frame from CameraService
    Frame frame;
    if (!camera_.grabFrame(frame, 100)) {
        return false;
    }

    result.frame_id = frame.frame_id;

    // 2. Inference via InferenceService (YOLOv8 TensorRT)
    auto detections = inference_.detect(frame);
    result.inference_ms = inference_.getLastInferenceLatencyMs();

    // 3. Object Tracking via TrackerService (ByteTrack)
    result.objects = tracker_.update(detections);

    // 4. Update Pipeline FPS
    frame_count_++;
    fps_counter_++;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<float>(now - fps_timer_).count();
    if (elapsed >= 1.0f) {
        current_fps_ = static_cast<float>(fps_counter_) / elapsed;
        fps_counter_ = 0;
        fps_timer_ = now;
    }
    result.fps = current_fps_;

    return true;
}

} // namespace edge_ai

