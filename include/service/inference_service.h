#pragma once

#include "app/types.h"
#include "hal/i_gpu_hal.h"
#include "service/preprocessor.h"
#include "service/postprocessor.h"
#include <memory>
#include <vector>

namespace edge_ai {

class InferenceService {
public:
    explicit InferenceService(hal::IGpuHAL& gpu_hal);
    ~InferenceService();

    bool loadModel(const ModelConfig& config);
    std::vector<Detection> detect(const Frame& frame);

    float getLastInferenceLatencyMs() const { return last_latency_ms_; }
    bool isReady() const { return ready_; }

private:
    hal::IGpuHAL& gpu_;
    Preprocessor preprocessor_;
    PostProcessor postprocessor_;
    ModelConfig config_;

    hal::GpuBuffer input_buffer_{nullptr};
    hal::GpuBuffer output_buffer_{nullptr};
    size_t input_bytes_{0};
    size_t output_bytes_{0};

    std::vector<float> host_input_chw_;
    std::vector<float> host_output_tensor_;

    float last_latency_ms_{0.0f};
    bool ready_{false};
};

} // namespace edge_ai

