#include "service/tracker_service.h"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "[Test] Running ByteTrack TrackerService test..." << std::endl;

    edge_ai::TrackerConfig cfg;
    cfg.max_age = 5;
    cfg.min_hits = 2;
    cfg.iou_thresh = 0.3f;

    edge_ai::TrackerService tracker(cfg);

    // Frame 1: Detection appears at (100, 100, 50, 50)
    edge_ai::Detection d1;
    d1.bbox = {100.0f, 100.0f, 50.0f, 50.0f};
    d1.confidence = 0.90f;
    d1.class_id = 0;
    d1.class_name = "person";

    std::vector<edge_ai::TrackedObject> res1 = tracker.update({d1});
    // With min_hits = 2, first frame track is unconfirmed
    assert(res1.empty());

    // Frame 2: Same object slightly shifted to (105, 105, 50, 50)
    edge_ai::Detection d2 = d1;
    d2.bbox = {105.0f, 105.0f, 50.0f, 50.0f};

    std::vector<edge_ai::TrackedObject> res2 = tracker.update({d2});
    assert(res2.size() == 1);
    int assigned_id = res2[0].track_id;
    assert(assigned_id > 0);
    std::cout << "  Track confirmed with ID: " << assigned_id << std::endl;

    // Frame 3: Object continues trajectory to (110, 110, 50, 50)
    edge_ai::Detection d3 = d1;
    d3.bbox = {110.0f, 110.0f, 50.0f, 50.0f};

    std::vector<edge_ai::TrackedObject> res3 = tracker.update({d3});
    assert(res3.size() == 1);
    assert(res3[0].track_id == assigned_id && "Track ID must remain persistent across frames");

    std::cout << "[Test] TrackerService test PASSED!" << std::endl;
    return 0;
}

