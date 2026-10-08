#include "service/inference_service.h"
#include <iostream>

namespace edge_ai {

InferenceService::InferenceService(hal::IGpuHAL& gpu_hal)
    : gpu_(gpu_hal) {
}

InferenceService::~InferenceService() {
    if (input_buffer_) gpu_.free(input_buffer_);
    if (output_buffer_) gpu_.free(output_buffer_);
}

bool InferenceService::loadModel(const ModelConfig& config) {
    config_ = config;

    preprocessor_ = Preprocessor(config.input_size);
    postprocessor_ = PostProcessor(config.conf_thresh, config.nms_thresh, config.class_file);

    if (!gpu_.initialize()) {
        return false;
    }

    hal::EngineConfig eng_cfg;
    eng_cfg.precision = config.precision;
    eng_cfg.max_batch_size = 1;

    if (!gpu_.loadEngine(config.onnx_path, config.engine_path, eng_cfg)) {
        return false;
    }

    hal::Dims in_dims = gpu_.getInputDims();
    hal::Dims out_dims = gpu_.getOutputDims();

    input_bytes_ = in_dims.elementCount() * sizeof(float);
    output_bytes_ = out_dims.elementCount() * sizeof(float);

    input_buffer_ = gpu_.allocate(input_bytes_);
    output_buffer_ = gpu_.allocate(output_bytes_);

    if (!input_buffer_ || !output_buffer_) {
        return false;
    }

    host_input_chw_.resize(in_dims.elementCount());
    host_output_tensor_.resize(out_dims.elementCount());

    ready_ = true;
    return true;
}

std::vector<Detection> InferenceService::detect(const Frame& frame) {
    if (!ready_ || frame.image.empty()) return {};

    // 1. Preprocess: letterbox + normalize
    PreprocessMeta meta;
    if (!preprocessor_.process(frame.image, host_input_chw_, meta)) {
        return {};
    }

    // 2. Transfer host -> device
    gpu_.copyHostToDevice(host_input_chw_.data(), input_buffer_, input_bytes_);

    // 3. TensorRT execution
    last_latency_ms_ = gpu_.runInference(input_buffer_, output_buffer_);

    // 4. Transfer device -> host
    gpu_.copyDeviceToHost(output_buffer_, host_output_tensor_.data(), output_bytes_);

    // 5. Postprocess: decode bounding boxes & NMS
    return postprocessor_.decode(host_output_tensor_.data(), meta);
}

} // namespace edge_ai

