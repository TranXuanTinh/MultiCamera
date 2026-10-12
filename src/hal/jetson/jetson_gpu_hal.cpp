#include "jetson_gpu_hal.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <cstring>

#if defined(ENABLE_JETSON_CUDA)
#include <NvOnnxParser.h>
#endif

namespace edge_ai { namespace hal {

#if defined(ENABLE_JETSON_CUDA)
void JetsonGpuHAL::Logger::log(Severity severity, const char* msg) noexcept {
    if (severity <= Severity::kWARNING) {
        std::cerr << "[TensorRT] " << msg << std::endl;
    }
}
#endif

JetsonGpuHAL::JetsonGpuHAL() = default;

JetsonGpuHAL::~JetsonGpuHAL() {
    destroy();
}

bool JetsonGpuHAL::initialize() {
#if defined(ENABLE_JETSON_CUDA)
    cudaError_t err = cudaStreamCreate(&stream_);
    return (err == cudaSuccess);
#else
    return true;
#endif
}

void JetsonGpuHAL::destroy() {
#if defined(ENABLE_JETSON_CUDA)
    if (stream_) {
        cudaStreamDestroy(stream_);
        stream_ = nullptr;
    }
    context_.reset();
    engine_.reset();
    runtime_.reset();
#endif
    loaded_ = false;
}

GpuBuffer JetsonGpuHAL::allocate(size_t bytes) {
#if defined(ENABLE_JETSON_CUDA)
    void* ptr = nullptr;
    if (cudaMalloc(&ptr, bytes) == cudaSuccess) {
        return ptr;
    }
    return nullptr;
#else
    return std::malloc(bytes);
#endif
}

void JetsonGpuHAL::free(GpuBuffer buf) {
#if defined(ENABLE_JETSON_CUDA)
    if (buf) {
        cudaFree(buf);
    }
#else
    std::free(buf);
#endif
}

bool JetsonGpuHAL::copyHostToDevice(const void* host_src, GpuBuffer dev_dst, size_t bytes) {
#if defined(ENABLE_JETSON_CUDA)
    return (cudaMemcpyAsync(dev_dst, host_src, bytes, cudaMemcpyHostToDevice, stream_) == cudaSuccess);
#else
    if (!host_src || !dev_dst) return false;
    std::memcpy(dev_dst, host_src, bytes);
    return true;
#endif
}

bool JetsonGpuHAL::copyDeviceToHost(GpuBuffer dev_src, void* host_dst, size_t bytes) {
#if defined(ENABLE_JETSON_CUDA)
    return (cudaMemcpyAsync(host_dst, dev_src, bytes, cudaMemcpyDeviceToHost, stream_) == cudaSuccess);
#else
    if (!dev_src || !host_dst) return false;
    std::memcpy(host_dst, dev_src, bytes);
    return true;
#endif
}

bool JetsonGpuHAL::loadEngine(const std::string& onnx_path,
                              const std::string& engine_cache_path,
                              const EngineConfig& config) {
    input_dims_.shape = {1, 3, 640, 640};
    output_dims_.shape = {1, 84, 8400};

#if defined(ENABLE_JETSON_CUDA)
    runtime_.reset(nvinfer1::createInferRuntime(logger_));
    if (!runtime_) return false;

    // 1. Try to load cached serialized engine
    std::ifstream file(engine_cache_path, std::ios::binary);
    if (file.good()) {
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> buffer(size);
        file.read(buffer.data(), size);
        file.close();

        engine_.reset(runtime_->deserializeCudaEngine(buffer.data(), size));
        if (engine_) {
            context_.reset(engine_->createExecutionContext());
            loaded_ = (context_ != nullptr);
            return loaded_;
        }
    }

    // 2. Build from ONNX if cache not available
    std::unique_ptr<nvinfer1::IBuilder> builder = std::unique_ptr<nvinfer1::IBuilder>(nvinfer1::createInferBuilder(logger_));
    if (!builder) return false;

    const uint32_t explicitBatch = 1U << static_cast<uint32_t>(nvinfer1::NetworkDefinitionCreationFlag::kEXPLICIT_BATCH);
    std::unique_ptr<nvinfer1::INetworkDefinition> network = std::unique_ptr<nvinfer1::INetworkDefinition>(builder->createNetworkV2(explicitBatch));
    std::unique_ptr<nvonnxparser::IParser> parser = std::unique_ptr<nvonnxparser::IParser>(nvonnxparser::createParser(*network, logger_));

    if (!parser->parseFromFile(onnx_path.c_str(), static_cast<int>(nvinfer1::ILogger::Severity::kWARNING))) {
        return false;
    }

    std::unique_ptr<nvinfer1::IBuilderConfig> builderConfig = std::unique_ptr<nvinfer1::IBuilderConfig>(builder->createBuilderConfig());
    builderConfig->setMaxWorkspaceSize(config.max_workspace_bytes);

    if (config.precision == "fp16" && builder->platformHasFastFp16()) {
        builderConfig->setFlag(nvinfer1::BuilderFlag::kFP16);
    }

    std::unique_ptr<nvinfer1::IHostMemory> plan = std::unique_ptr<nvinfer1::IHostMemory>(builder->buildSerializedNetwork(*network, *builderConfig));
    if (!plan) return false;

    // Save serialized engine to cache file
    std::ofstream outfile(engine_cache_path, std::ios::binary);
    if (outfile.good()) {
        outfile.write(reinterpret_cast<const char*>(plan->data()), plan->size());
        outfile.close();
    }

    engine_.reset(runtime_->deserializeCudaEngine(plan->data(), plan->size()));
    if (!engine_) return false;

    context_.reset(engine_->createExecutionContext());
    loaded_ = (context_ != nullptr);
    return loaded_;
#else
    (void)onnx_path;
    (void)engine_cache_path;
    (void)config;
    loaded_ = true;
    return true;
#endif
}

float JetsonGpuHAL::runInference(GpuBuffer input_buffer, GpuBuffer output_buffer) {
#if defined(ENABLE_JETSON_CUDA)
    if (!context_ || !stream_) return 0.0f;

    void* bindings[2] = {input_buffer, output_buffer};
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    context_->enqueueV2(bindings, stream_, nullptr);
    cudaStreamSynchronize(stream_);

    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    return std::chrono::duration<float, std::milli>(end - start).count();
#else
    (void)input_buffer;
    (void)output_buffer;
    return 15.0f;
#endif
}

Dims JetsonGpuHAL::getInputDims() const {
    return input_dims_;
}

Dims JetsonGpuHAL::getOutputDims() const {
    return output_dims_;
}

bool JetsonGpuHAL::isEngineLoaded() const {
    return loaded_;
}

}} // namespace edge_ai::hal

