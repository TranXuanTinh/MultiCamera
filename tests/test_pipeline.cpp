#include "app/pipeline.h"
#include "service/camera_service.h"
#include "service/inference_service.h"
#include "service/tracker_service.h"
#include "hal/mock/mock_camera_hal.h"
#include "hal/mock/mock_gpu_hal.h"
#include "hal/mock/mock_deepstream_hal.h"
#include <iostream>
#include <cassert>
#include <set>

int main() {
    std::cout << "[Test] Running Multi-Camera DeepStream & Pipeline tests..." << std::endl;

    // =========================================================================
    // Part 1: DeepStream Multi-Camera Pipeline Test (Mock HAL)
    // =========================================================================
    {
        std::cout << "  --> Testing DeepStream Multi-Camera Pipeline with 2 cameras..." << std::endl;

        edge_ai::hal::MockDeepStreamHAL ds_hal;
        edge_ai::DeepStreamConfig ds_cfg;
        ds_cfg.batch_size = 2;
        ds_cfg.batched_push_timeout_us = 40000;
        ds_cfg.muxer_width = 1280;
        ds_cfg.muxer_height = 720;

        std::vector<edge_ai::CameraSourceConfig> cameras = {
            {"cam_gate", "Front Gate", "csi", "/dev/video0", 0, 0, 1280, 720, 30, true, 5},
            {"cam_warehouse", "Warehouse Lane", "rtsp", "rtsp://192.168.1.100/stream", 1, 0, 1280, 720, 30, true, 5}
        };

        assert(ds_hal.initialize(ds_cfg, cameras) && "DeepStream HAL init failed");
        assert(ds_hal.getActiveSourceCount() == 2 && "Must have 2 active camera sources");

        edge_ai::Pipeline ds_pipeline(ds_hal);
        assert(ds_pipeline.isDeepStream() && "Pipeline must identify as DeepStream mode");
        assert(ds_pipeline.start() && "DeepStream pipeline start failed");

        std::set<std::string> received_cameras;
        int callback_count = 0;

        ds_pipeline.setResultCallback([&](const edge_ai::PipelineResult& res) {
            received_cameras.insert(res.camera_id);
            callback_count++;
            std::cout << "      Received metadata from camera: " << res.camera_id
                      << " (source_id: " << res.source_id << "), objects: "
                      << res.objects.size() << std::endl;
        });

        // Step 6 frames through the multi-camera pipeline
        for (int i = 0; i < 6; ++i) {
            edge_ai::PipelineResult res;
            bool ok = ds_pipeline.processOneFrame(res);
            assert(ok && "Step frame failed");
            assert(res.batch_size == 2 && "Batch size must be 2");
            assert(!res.objects.empty() && "Mock objects must be present");
            assert(res.camera_id == "cam_gate" || res.camera_id == "cam_warehouse");
        }

        assert(received_cameras.count("cam_gate") == 1 && "Must receive metadata from cam_gate");
        assert(received_cameras.count("cam_warehouse") == 1 && "Must receive metadata from cam_warehouse");

        ds_pipeline.stop();
        std::cout << "  --> DeepStream Multi-Camera pipeline test PASSED!" << std::endl;
    }

    // =========================================================================
    // Part 2: Legacy Single-Camera Pipeline Test (Backward Compatibility)
    // =========================================================================
    {
        std::cout << "  --> Testing Legacy Single-Camera Pipeline compatibility..." << std::endl;

        edge_ai::hal::MockCameraHAL cam_hal;
        edge_ai::hal::MockGpuHAL gpu_hal;

        edge_ai::CameraConfig cam_cfg;
        cam_cfg.width = 1280;
        cam_cfg.height = 720;
        cam_cfg.fps = 30;

        edge_ai::ModelConfig model_cfg;
        model_cfg.input_size = 640;
        model_cfg.conf_thresh = 0.45f;
        model_cfg.nms_thresh = 0.50f;

        edge_ai::TrackerConfig trk_cfg;
        trk_cfg.max_age = 10;
        trk_cfg.min_hits = 1;

        edge_ai::CameraService cam_svc(cam_hal);
        assert(cam_svc.open(cam_cfg) && "CameraService open failed");

        edge_ai::InferenceService inf_svc(gpu_hal);
        assert(inf_svc.loadModel(model_cfg) && "InferenceService loadModel failed");

        edge_ai::TrackerService trk_svc(trk_cfg);

        edge_ai::Pipeline pipeline(cam_svc, inf_svc, trk_svc);
        assert(!pipeline.isDeepStream() && "Pipeline must identify as legacy mode");
        assert(pipeline.start() && "Pipeline start failed");

        for (int i = 0; i < 3; ++i) {
            edge_ai::PipelineResult res;
            bool ok = pipeline.processOneFrame(res);
            assert(ok && "Pipeline processOneFrame failed");
            assert(res.frame_id > 0 && "Frame ID must be positive");
            assert(!res.objects.empty() && "Mock should produce tracked objects");
        }

        pipeline.stop();
        cam_svc.close();
        std::cout << "  --> Legacy Single-Camera test PASSED!" << std::endl;
    }

    std::cout << "[Test] All Pipeline integration tests PASSED!" << std::endl;
    return 0;
}
