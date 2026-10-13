#include "app/rule_engine.h"
#include "app/rules/zone_rule.h"
#include "app/rules/counting_rule.h"
#include "service/event_bus.h"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "[Test] Running RuleEngine & Multi-Camera Domain Rules test..." << std::endl;

    edge_ai::EventBus event_bus(32);
    event_bus.start();

    edge_ai::RuleEngine engine(event_bus);

    // 1. Setup polygon safety zone for cam_01: [0,0] to [200, 200]
    std::vector<cv::Point2f> zone = {
        {0.0f, 0.0f}, {200.0f, 0.0f}, {200.0f, 200.0f}, {0.0f, 200.0f}
    };
    engine.addRule(std::make_unique<edge_ai::ZoneRule>(
        "r1", "Test Zone Cam1", std::vector<std::string>{"person"}, zone, "high", "Intrusion Cam1", 0, "cam_01"
    ));

    // 2. Setup counting line for cam_02: crossing y = 300
    engine.addRule(std::make_unique<edge_ai::CountingRule>(
        "r2", "Test Line Cam2", std::vector<std::string>{"car"},
        cv::Point2f(0.0f, 300.0f), cv::Point2f(500.0f, 300.0f), "both", "info", "Vehicle cross Cam2", "cam_02"
    ));

    // Test 1: Object inside zone geometry but from cam_02 (rule r1 is for cam_01)
    edge_ai::TrackedObject inside_cam2;
    inside_cam2.track_id = 1;
    inside_cam2.camera_id = "cam_02";
    inside_cam2.det.bbox = {50.0f, 50.0f, 50.0f, 50.0f};
    inside_cam2.det.class_name = "person";
    inside_cam2.det.camera_id = "cam_02";

    std::vector<edge_ai::Alert> alerts1 = engine.evaluate({inside_cam2});
    assert(alerts1.empty() && "Rule for cam_01 must not trigger for cam_02 object");

    // Test 2: Same object coordinates but from cam_01
    edge_ai::TrackedObject inside_cam1;
    inside_cam1.track_id = 2;
    inside_cam1.camera_id = "cam_01";
    inside_cam1.det.bbox = {50.0f, 50.0f, 50.0f, 50.0f};
    inside_cam1.det.class_name = "person";
    inside_cam1.det.camera_id = "cam_01";

    std::vector<edge_ai::Alert> alerts2 = engine.evaluate({inside_cam1});
    assert(alerts2.size() == 1 && "Alert must trigger for cam_01 object in cam_01 zone");
    assert(alerts2[0].rule_id == "r1");
    assert(alerts2[0].camera_id == "cam_01");
    std::cout << "  Multi-camera zone rule verified: [" << alerts2[0].camera_id << "] "
              << alerts2[0].message << std::endl;

    // Test 3: Vehicle crossing line on cam_02
    edge_ai::TrackedObject car_frame1;
    car_frame1.track_id = 10;
    car_frame1.camera_id = "cam_02";
    car_frame1.det.class_name = "car";
    car_frame1.det.bbox = {250.0f, 250.0f, 60.0f, 60.0f}; // Above y=300
    engine.evaluate({car_frame1});

    edge_ai::TrackedObject car_frame2 = car_frame1;
    car_frame2.det.bbox = {250.0f, 350.0f, 60.0f, 60.0f}; // Below y=300 (crossed)
    std::vector<edge_ai::Alert> alerts_cross = engine.evaluate({car_frame2});
    assert(alerts_cross.size() == 1 && "Counting rule must trigger on line crossing");
    assert(alerts_cross[0].rule_id == "r2");
    assert(alerts_cross[0].camera_id == "cam_02");
    std::cout << "  Multi-camera line crossing verified: [" << alerts_cross[0].camera_id << "] "
              << alerts_cross[0].message << std::endl;

    event_bus.stop();
    std::cout << "[Test] RuleEngine & Multi-Camera tests PASSED!" << std::endl;
    return 0;
}
