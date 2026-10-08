#include "app/pipeline.h"
#include "service/camera_service.h"
#include "service/inference_service.h"
#include "service/tracker_service.h"
#include "hal/mock/mock_camera_hal.h"
#include "hal/mock/mock_gpu_hal.h"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "[Test] Running Pipeline integration test with Mock HAL..." << std::endl;

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
    trk_cfg.min_hits = 1; // Immediate confirmation for testing

    edge_ai::CameraService cam_svc(cam_hal);
    assert(cam_svc.open(cam_cfg) && "CameraService open failed");

    edge_ai::InferenceService inf_svc(gpu_hal);
    assert(inf_svc.loadModel(model_cfg) && "InferenceService loadModel failed");

    edge_ai::TrackerService trk_svc(trk_cfg);

    edge_ai::Pipeline pipeline(cam_svc, inf_svc, trk_svc);
    assert(pipeline.start() && "Pipeline start failed");

    // Process 5 frames
    for (int i = 0; i < 5; ++i) {
        edge_ai::PipelineResult res;
        bool ok = pipeline.processOneFrame(res);
        assert(ok && "Pipeline processOneFrame failed");
        assert(res.frame_id > 0 && "Frame ID must be positive");
        assert(!res.objects.empty() && "Mock should produce tracked objects");
        std::cout << "  Frame " << res.frame_id
                  << " processed: " << res.objects.size()
                  << " tracked object(s), latency=" << res.inference_ms << "ms" << std::endl;
    }

    pipeline.stop();
    cam_svc.close();

    std::cout << "[Test] Pipeline integration test PASSED!" << std::endl;
    return 0;
}

