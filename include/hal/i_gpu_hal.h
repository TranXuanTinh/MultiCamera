#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace edge_ai { namespace hal {

using GpuBuffer = void*; // Opaque handle for device pointer

struct Dims {
    std::vector<int> shape; // e.g. {1, 3, 640, 640} or {1, 84, 8400}
    size_t elementCount() const {
        if (shape.empty()) return 0;
        size_t count = 1;
        for (int d : shape) count *= (d > 0 ? d : 1);
        return count;
    }
};

struct EngineConfig {
    std::string precision{"fp16"}; // "fp16" | "fp32"
    int max_batch_size{1};
    size_t max_workspace_bytes{128 * 1024 * 1024}; // 128 MB on Jetson Nano
};

class IGpuHAL {
public:
    virtual ~IGpuHAL() = default;

    virtual bool initialize() = 0;
    virtual void destroy() = 0;

    // GPU Memory Management
    virtual GpuBuffer allocate(size_t bytes) = 0;
    virtual void free(GpuBuffer buf) = 0;
    virtual bool copyHostToDevice(const void* host_src, GpuBuffer dev_dst, size_t bytes) = 0;
    virtual bool copyDeviceToHost(GpuBuffer dev_src, void* host_dst, size_t bytes) = 0;

    // Model Engine & Execution
    virtual bool loadEngine(const std::string& onnx_path,
                            const std::string& engine_cache_path,
                            const EngineConfig& config) = 0;
    virtual float runInference(GpuBuffer input_buffer, GpuBuffer output_buffer) = 0;

    virtual Dims getInputDims() const = 0;
    virtual Dims getOutputDims() const = 0;
    virtual bool isEngineLoaded() const = 0;
};

}} // namespace edge_ai::hal

