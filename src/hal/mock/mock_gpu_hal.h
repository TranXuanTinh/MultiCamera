#pragma once

#include "hal/i_gpu_hal.h"
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <vector>

namespace edge_ai { namespace hal {

class MockGpuHAL : public IGpuHAL {
public:
    MockGpuHAL() = default;
    ~MockGpuHAL() override { destroy(); }

    bool initialize() override { return true; }

    void destroy() override {
        for (void* ptr : allocated_blocks_) {
            std::free(ptr);
        }
        allocated_blocks_.clear();
    }

    GpuBuffer allocate(size_t bytes) override {
        void* ptr = std::malloc(bytes);
        if (ptr) {
            std::memset(ptr, 0, bytes);
            allocated_blocks_.push_back(ptr);
        }
        return ptr;
    }

    void free(GpuBuffer buf) override {
        auto it = std::find(allocated_blocks_.begin(), allocated_blocks_.end(), buf);
        if (it != allocated_blocks_.end()) {
            std::free(buf);
            allocated_blocks_.erase(it);
        }
    }

    bool copyHostToDevice(const void* host_src, GpuBuffer dev_dst, size_t bytes) override {
        if (!host_src || !dev_dst) return false;
        std::memcpy(dev_dst, host_src, bytes);
        return true;
    }

    bool copyDeviceToHost(GpuBuffer dev_src, void* host_dst, size_t bytes) override {
        if (!dev_src || !host_dst) return false;
        std::memcpy(host_dst, dev_src, bytes);
        return true;
    }

    bool loadEngine(const std::string& /*onnx_path*/,
                    const std::string& /*engine_cache_path*/,
                    const EngineConfig& /*config*/) override {
        input_dims_.shape = {1, 3, 640, 640};
        output_dims_.shape = {1, 84, 8400};
        loaded_ = true;
        return true;
    }

    float runInference(GpuBuffer /*input_buffer*/, GpuBuffer output_buffer) override {
        // Simulate inference latency (~40ms on Jetson Nano)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        if (!output_buffer) return 20.0f;

        // Generate synthetic YOLOv8 output tensor: [1, 84, 8400]
        // 84 channels: [cx, cy, w, h, class0_prob, class1_prob, ..., class79_prob]
        float* out = static_cast<float*>(output_buffer);
        size_t total_elements = 84 * 8400;
        std::memset(out, 0, total_elements * sizeof(float));

        // Inject 1 simulated detection at anchor index 0:
        // Person (class_id 0) at center (320, 320), size (150, 300)
        out[0 * 8400 + 0] = 320.0f; // cx
        out[1 * 8400 + 0] = 320.0f; // cy
        out[2 * 8400 + 0] = 150.0f; // w
        out[3 * 8400 + 0] = 300.0f; // h
        out[(4 + 0) * 8400 + 0] = 0.92f; // person confidence

        return 20.0f;
    }

    Dims getInputDims() const override { return input_dims_; }
    Dims getOutputDims() const override { return output_dims_; }
    bool isEngineLoaded() const override { return loaded_; }

private:
    bool loaded_{false};
    Dims input_dims_;
    Dims output_dims_;
    std::vector<void*> allocated_blocks_;
};

}} // namespace edge_ai::hal

