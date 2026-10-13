#include "app/rule_engine.h"
#include "app/rules/zone_rule.h"
#include "app/rules/counting_rule.h"
#include <iostream>

#if defined(ENABLE_YAML_CPP)
#include <yaml-cpp/yaml.h>
#endif

namespace edge_ai {

RuleEngine::RuleEngine(EventBus& event_bus)
    : event_bus_(event_bus) {
}

void RuleEngine::addRule(std::unique_ptr<IRule> rule) {
    if (rule) {
        rules_.push_back(std::move(rule));
    }
}

bool RuleEngine::loadRulesFromConfig(const std::string& rules_yaml_path) {
    rules_.clear();

#if defined(ENABLE_YAML_CPP)
    try {
        YAML::Node doc = YAML::LoadFile(rules_yaml_path);
        if (doc["rules"]) {
            for (const YAML::Node& item : doc["rules"]) {
                std::string id = item["id"].as<std::string>();
                std::string name = item["name"].as<std::string>();
                std::string type = item["type"].as<std::string>();
                bool enabled = item["enabled"].as<bool>(true);
                std::string camera_id = "*";
                if (item["camera_id"]) {
                    camera_id = item["camera_id"].as<std::string>();
                }

                std::vector<std::string> classes;
                if (item["params"]["target_classes"]) {
                    for (const YAML::Node& c : item["params"]["target_classes"]) {
                        classes.push_back(c.as<std::string>());
                    }
                }

                std::string severity = "info";
                std::string message = "Rule triggered";
                int cooldown = 10;
                if (item["alert"]) {
                    if (item["alert"]["severity"]) severity = item["alert"]["severity"].as<std::string>();
                    if (item["alert"]["message"]) message = item["alert"]["message"].as<std::string>();
                    if (item["alert"]["cooldown_sec"]) cooldown = item["alert"]["cooldown_sec"].as<int>();
                }

                if (type == "zone_intrusion") {
                    std::vector<cv::Point2f> pts;
                    if (item["params"]["zone"]["points"]) {
                        for (const YAML::Node& pt : item["params"]["zone"]["points"]) {
                            pts.push_back(cv::Point2f(pt[0].as<float>(), pt[1].as<float>()));
                        }
                    }
                    std::unique_ptr<ZoneRule> rule = std::make_unique<ZoneRule>(
                        id, name, classes, pts, severity, message, cooldown, camera_id);
                    rule->setEnabled(enabled);
                    addRule(std::move(rule));
                } else if (type == "line_crossing") {
                    cv::Point2f p1(100.0f, 400.0f), p2(1180.0f, 400.0f);
                    if (item["params"]["line"]["start"]) {
                        p1 = cv::Point2f(item["params"]["line"]["start"][0].as<float>(),
                                         item["params"]["line"]["start"][1].as<float>());
                    }
                    if (item["params"]["line"]["end"]) {
                        p2 = cv::Point2f(item["params"]["line"]["end"][0].as<float>(),
                                         item["params"]["line"]["end"][1].as<float>());
                    }
                    std::string direction = item["params"]["direction"].as<std::string>("both");
                    std::unique_ptr<CountingRule> rule = std::make_unique<CountingRule>(
                        id, name, classes, p1, p2, direction, severity, message, camera_id);
                    rule->setEnabled(enabled);
                    addRule(std::move(rule));
                }
            }
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[RuleEngine] Failed to parse " << rules_yaml_path << ": " << e.what() << std::endl;
    }
#endif

    // Fallback default rules if YAML parser is disabled or file not found
    std::vector<cv::Point2f> default_zone = {
        cv::Point2f(200.0f, 150.0f),
        cv::Point2f(600.0f, 150.0f),
        cv::Point2f(600.0f, 550.0f),
        cv::Point2f(200.0f, 550.0f)
    };
    addRule(std::make_unique<ZoneRule>(
        "rule_zone_default", "Default Person Restricted Area",
        std::vector<std::string>{"person"}, default_zone, "high",
        "Person detected in restricted safety zone", 10, "*"
    ));

    return true;
}

std::vector<Alert> RuleEngine::evaluate(const std::vector<TrackedObject>& objects) {
    std::vector<Alert> alerts;

    for (const std::unique_ptr<IRule>& rule : rules_) {
        if (!rule->isEnabled()) continue;

        std::optional<Alert> maybe_alert = rule->evaluate(objects);
        if (maybe_alert.has_value()) {
            alerts.push_back(maybe_alert.value());

            // Fire event to EventBus
            Event evt;
            evt.type = EventType::ALERT;
            evt.timestamp = std::chrono::system_clock::now();
            evt.data = maybe_alert.value();
            event_bus_.publish(std::move(evt));
        }
    }

    return alerts;
}

} // namespace edge_ai

