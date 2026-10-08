#include "app/rule_engine.h"
#include "app/rules/zone_rule.h"
#include "app/rules/counting_rule.h"
#include "service/event_bus.h"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "[Test] Running RuleEngine & Domain Rules test..." << std::endl;

    edge_ai::EventBus event_bus(32);
    event_bus.start();

    edge_ai::RuleEngine engine(event_bus);

    // 1. Setup polygon safety zone: [0,0] to [200, 200]
    std::vector<cv::Point2f> zone = {
        {0.0f, 0.0f}, {200.0f, 0.0f}, {200.0f, 200.0f}, {0.0f, 200.0f}
    };
    engine.addRule(std::make_unique<edge_ai::ZoneRule>(
        "r1", "Test Zone", std::vector<std::string>{"person"}, zone, "high", "Intrusion", 0
    ));

    // 2. Setup counting line: crossing y = 300
    engine.addRule(std::make_unique<edge_ai::CountingRule>(
        "r2", "Test Line", std::vector<std::string>{"car"},
        cv::Point2f(0.0f, 300.0f), cv::Point2f(500.0f, 300.0f), "both"
    ));

    // Test Object outside zone
    edge_ai::TrackedObject outside_obj;
    outside_obj.track_id = 1;
    outside_obj.det.bbox = {400.0f, 400.0f, 50.0f, 50.0f};
    outside_obj.det.class_name = "person";

    auto alerts1 = engine.evaluate({outside_obj});
    assert(alerts1.empty() && "No alert should trigger when outside zone");

    // Test Object inside zone
    edge_ai::TrackedObject inside_obj;
    inside_obj.track_id = 2;
    inside_obj.det.bbox = {50.0f, 50.0f, 50.0f, 50.0f};
    inside_obj.det.class_name = "person";

    auto alerts2 = engine.evaluate({inside_obj});
    assert(alerts2.size() == 1 && "Alert must trigger when inside zone");
    assert(alerts2[0].rule_id == "r1");
    std::cout << "  Intrusion alert verified: " << alerts2[0].message << std::endl;

    event_bus.stop();
    std::cout << "[Test] RuleEngine test PASSED!" << std::endl;
    return 0;
}

