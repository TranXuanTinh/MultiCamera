#pragma once

#include "app/rules/i_rule.h"
#include <opencv2/core.hpp>
#include <map>
#include <chrono>

namespace edge_ai {

class CountingRule : public IRule {
public:
    CountingRule(const std::string& id,
                 const std::string& name,
                 const std::vector<std::string>& target_classes,
                 cv::Point2f p1, cv::Point2f p2,
                 const std::string& direction = "both",
                 const std::string& severity = "info",
                 const std::string& message = "Line crossed",
                 const std::string& camera_id = "*");

    ~CountingRule() override = default;

    std::string getId() const override { return id_; }
    std::string getName() const override { return name_; }
    std::string getType() const override { return "line_crossing"; }
    std::string getCameraId() const override { return camera_id_; }
    bool isEnabled() const override { return enabled_; }
    void setEnabled(bool enabled) override { enabled_ = enabled; }

    std::optional<Alert> evaluate(const std::vector<TrackedObject>& objects) override;
    void resetCameraState(const std::string& camera_id) override;

    int getTotalCount() const { return total_count_; }

private:
    float crossProduct(cv::Point2f a, cv::Point2f b, cv::Point2f c) const;

    std::string id_;
    std::string name_;
    std::vector<std::string> target_classes_;
    cv::Point2f p1_;
    cv::Point2f p2_;
    std::string direction_;
    std::string severity_;
    std::string message_;
    std::string camera_id_{"*"};
    bool enabled_{true};

    int total_count_{0};
    std::map<std::pair<std::string, int>, float> previous_side_;
};

} // namespace edge_ai

