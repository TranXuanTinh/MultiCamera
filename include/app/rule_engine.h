#pragma once

#include "app/types.h"
#include "app/rules/i_rule.h"
#include "service/event_bus.h"
#include <vector>
#include <memory>
#include <string>

namespace edge_ai {

class RuleEngine {
public:
    explicit RuleEngine(EventBus& event_bus);
    ~RuleEngine() = default;

    bool loadRulesFromConfig(const std::string& rules_yaml_path);
    void addRule(std::unique_ptr<IRule> rule);

    std::vector<Alert> evaluate(const std::vector<TrackedObject>& objects);

    size_t getRuleCount() const { return rules_.size(); }

private:
    EventBus& event_bus_;
    std::vector<std::unique_ptr<IRule>> rules_;
};

} // namespace edge_ai

