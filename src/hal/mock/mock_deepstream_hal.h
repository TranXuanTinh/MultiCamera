#pragma once

#include "hal/i_deepstream_hal.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cmath>
#include <iostream>

namespace edge_ai { namespace hal {

class MockDeepStreamHAL : public IDeepStreamHAL {
public:
    MockDeepStreamHAL() = default;
    ~MockDeepStreamHAL() override {
        stop();
    }

    bool initialize(const DeepStreamConfig& ds_config,
                    const std::vector<CameraSourceConfig>& cameras) override {
        ds_config_ = ds_config;
        cameras_ = cameras;
        if (cameras_.empty()) {
            // Default 2 simulated cameras if empty
            CameraSourceConfig c1{"cam_01", "Front Gate (Simulated)", "csi", "/dev/video0", 0, 0, 1280, 720, 30, true, 5};
            CameraSourceConfig c2{"cam_02", "Warehouse (Simulated)", "rtsp", "rtsp://mock/stream2", 1, 0, 1280, 720, 30, true, 5};
            cameras_.push_back(c1);
            cameras_.push_back(c2);
        }
        initialized_ = true;
        return true;
    }

    bool start() override {
        if (!initialized_) return false;
        if (running_) return true;

        running_ = true;
        paused_ = false;
        worker_thread_ = std::thread(&MockDeepStreamHAL::simulateLoop, this);
        return true;
    }

    void pause() override {
        paused_ = true;
    }

    void resume() override {
        paused_ = false;
    }

    void stop() override {
        running_ = false;
        paused_ = false;
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    bool isRunning() const override {
        return running_;
    }

    void setMetadataCallback(MetadataCallback cb) override {
        std::lock_guard<std::mutex> lock(cb_mtx_);
        cb_ = std::move(cb);
    }

    float getStreamFps(int source_id) const override {
        (void)source_id;
        return 30.0f;
    }

    size_t getActiveSourceCount() const override {
        return cameras_.size();
    }

    bool stepFrame(PipelineResult& out_result) override {
        if (!initialized_ || cameras_.empty()) return false;

        int src_idx = static_cast<int>(step_counter_ % cameras_.size());
        const CameraSourceConfig& cam = cameras_[src_idx];
        generateSyntheticResult(cam, src_idx, ++step_counter_, out_result);

        MetadataCallback local_cb;
        {
            std::lock_guard<std::mutex> lock(cb_mtx_);
            local_cb = cb_;
        }
        if (local_cb) {
            local_cb(out_result);
        }
        return true;
    }

private:
    void simulateLoop() {
        uint64_t seq = 0;
        while (running_) {
            if (paused_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                continue;
            }

            // Simulate batch arrival every 33ms (30 FPS)
            auto start_time = std::chrono::steady_clock::now();

            for (size_t i = 0; i < cameras_.size(); ++i) {
                if (!cameras_[i].enabled) continue;

                PipelineResult res;
                generateSyntheticResult(cameras_[i], static_cast<int>(i), ++seq, res);

                MetadataCallback local_cb;
                {
                    std::lock_guard<std::mutex> lock(cb_mtx_);
                    local_cb = cb_;
                }
                if (local_cb) {
                    local_cb(res);
                }
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count();
            if (elapsed < 33) {
                std::this_thread::sleep_for(std::chrono::milliseconds(33 - elapsed));
            }
        }
    }

    void generateSyntheticResult(const CameraSourceConfig& cam, int src_idx, uint64_t seq, PipelineResult& res) {
        res.camera_id = cam.id;
        res.source_id = src_idx;
        res.frame_id = seq;
        res.batch_size = static_cast<uint32_t>(cameras_.size());
        res.inference_ms = 18.5f + static_cast<float>((seq % 5));
        res.fps = 29.8f;

        // Generate dynamic tracked objects per camera stream
        TrackedObject obj1;
        obj1.camera_id = cam.id;
        obj1.source_id = src_idx;
        obj1.track_id = 100 * (src_idx + 1) + 1; // Distinct track ID per camera
        obj1.state = TrackState::Confirmed;
        obj1.hits = 15;
        obj1.age = static_cast<int>(seq);
        obj1.time_since_update = 0;

        // Simulated motion
        float cx = 250.0f + 100.0f * std::sin(static_cast<float>(seq) * 0.1f + src_idx);
        float cy = 200.0f + 50.0f * std::cos(static_cast<float>(seq) * 0.1f + src_idx);
        obj1.det.bbox = {cx, cy, 60.0f, 140.0f};
        obj1.det.confidence = 0.88f;
        obj1.det.class_id = 0;
        obj1.det.class_name = "person";
        obj1.det.camera_id = cam.id;
        obj1.det.source_id = src_idx;
        obj1.vx = 2.5f;
        obj1.vy = 1.0f;

        res.objects.push_back(obj1);

        // For camera 2: add a moving vehicle
        if (src_idx == 1 || (cameras_.size() > 1 && src_idx % 2 == 1)) {
            TrackedObject obj2;
            obj2.camera_id = cam.id;
            obj2.source_id = src_idx;
            obj2.track_id = 100 * (src_idx + 1) + 2;
            obj2.state = TrackState::Confirmed;
            obj2.hits = 20;
            obj2.age = static_cast<int>(seq);
            obj2.time_since_update = 0;

            float vx_pos = 150.0f + static_cast<float>((seq * 15) % 900);
            obj2.det.bbox = {vx_pos, 420.0f, 180.0f, 100.0f};
            obj2.det.confidence = 0.92f;
            obj2.det.class_id = 2;
            obj2.det.class_name = "car";
            obj2.det.camera_id = cam.id;
            obj2.det.source_id = src_idx;
            obj2.vx = 5.0f;
            obj2.vy = 0.0f;

            res.objects.push_back(obj2);
        }
    }

    DeepStreamConfig ds_config_;
    std::vector<CameraSourceConfig> cameras_;
    std::atomic<bool> initialized_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};
    uint64_t step_counter_{0};

    std::thread worker_thread_;
    std::mutex cb_mtx_;
    MetadataCallback cb_;
};

}} // namespace edge_ai::hal

