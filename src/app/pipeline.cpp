#include "app/pipeline.h"

namespace edge_ai {

Pipeline::Pipeline(hal::IDeepStreamHAL& deepstream_hal)
    : ds_hal_(&deepstream_hal), deepstream_mode_(true) {
    ds_hal_->setMetadataCallback([this](const PipelineResult& res) {
        onDeepStreamMetadata(res);
    });
}

Pipeline::Pipeline(CameraService& camera,
                   InferenceService& inference,
                   TrackerService& tracker)
    : camera_(&camera), inference_(&inference), tracker_(&tracker), deepstream_mode_(false) {
}

Pipeline::~Pipeline() {
    stop();
}

bool Pipeline::start() {
    running_ = true;
    frame_count_ = 0;
    fps_timer_ = std::chrono::steady_clock::now();
    fps_counter_ = 0;

    if (deepstream_mode_ && ds_hal_) {
        return ds_hal_->start();
    }
    return true;
}

void Pipeline::stop() {
    running_ = false;
    if (deepstream_mode_ && ds_hal_) {
        ds_hal_->stop();
    }
}

void Pipeline::setResultCallback(ResultCallback cb) {
    std::lock_guard<std::mutex> lock(cb_mtx_);
    result_callback_ = std::move(cb);
}

void Pipeline::onDeepStreamMetadata(const PipelineResult& result) {
    if (!running_) return;

    frame_count_++;
    fps_counter_++;
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(now - fps_timer_).count();
    if (elapsed >= 1.0f) {
        current_fps_ = static_cast<float>(fps_counter_) / elapsed;
        fps_counter_ = 0;
        fps_timer_ = now;
    }

    PipelineResult res = result;
    res.fps = current_fps_;

    ResultCallback cb;
    {
        std::lock_guard<std::mutex> lock(cb_mtx_);
        cb = result_callback_;
    }
    if (cb) {
        cb(res);
    }
}

bool Pipeline::processOneFrame(PipelineResult& result) {
    if (!running_) return false;

    if (deepstream_mode_) {
        if (ds_hal_) {
            return ds_hal_->stepFrame(result);
        }
        return false;
    }

    if (!camera_ || !inference_ || !tracker_) return false;

    // Legacy Single Camera manual flow
    Frame frame;
    if (!camera_->grabFrame(frame, 100)) {
        return false;
    }

    result.frame_id = frame.frame_id;
    result.camera_id = frame.camera_id;
    result.source_id = frame.source_id;

    std::vector<Detection> detections = inference_->detect(frame);
    result.inference_ms = inference_->getLastInferenceLatencyMs();

    result.objects = tracker_->update(detections);
    for (TrackedObject& obj : result.objects) {
        obj.camera_id = result.camera_id;
        obj.source_id = result.source_id;
    }

    frame_count_++;
    fps_counter_++;
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(now - fps_timer_).count();
    if (elapsed >= 1.0f) {
        current_fps_ = static_cast<float>(fps_counter_) / elapsed;
        fps_counter_ = 0;
        fps_timer_ = now;
    }
    result.fps = current_fps_;

    return true;
}

} // namespace edge_ai
