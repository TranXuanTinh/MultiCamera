#pragma once

#include "hal/i_gpu_hal.h"
#include <memory>
#include <string>

#if defined(ENABLE_JETSON_CUDA)
#include <NvInfer.h>
#include <cuda_runtime_api.h>
#endif

namespace edge_ai { namespace hal {

class JetsonGpuHAL : public IGpuHAL {
public:
    JetsonGpuHAL();
    ~JetsonGpuHAL() override;

    bool initialize() override;
    void destroy() override;

    GpuBuffer allocate(size_t bytes) override;
    void free(GpuBuffer buf) override;
    bool copyHostToDevice(const void* host_src, GpuBuffer dev_dst, size_t bytes) override;
    bool copyDeviceToHost(GpuBuffer dev_src, void* host_dst, size_t bytes) override;

    bool loadEngine(const std::string& onnx_path,
                    const std::string& engine_cache_path,
                    const EngineConfig& config) override;

    float runInference(GpuBuffer input_buffer, GpuBuffer output_buffer) override;

    Dims getInputDims() const override;
    Dims getOutputDims() const override;
    bool isEngineLoaded() const override;

private:
#if defined(ENABLE_JETSON_CUDA)
    class Logger : public nvinfer1::ILogger {
        void log(Severity severity, const char* msg) noexcept override;
    } logger_;

    std::unique_ptr<nvinfer1::IRuntime> runtime_{nullptr};
    std::unique_ptr<nvinfer1::ICudaEngine> engine_{nullptr};
    std::unique_ptr<nvinfer1::IExecutionContext> context_{nullptr};
    cudaStream_t stream_{nullptr};
#endif

    Dims input_dims_;
    Dims output_dims_;
    bool loaded_{false};
};

}} // namespace edge_ai::hal

