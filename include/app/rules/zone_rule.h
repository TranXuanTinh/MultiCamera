#pragma once

#include "app/rules/i_rule.h"
#include <opencv2/core.hpp>
#include <chrono>

namespace edge_ai {

class ZoneRule : public IRule {
public:
    ZoneRule(const std::string& id,
             const std::string& name,
             const std::vector<std::string>& target_classes,
             const std::vector<cv::Point2f>& polygon_points,
             const std::string& severity = "high",
             const std::string& message = "Zone intrusion detected",
             int cooldown_sec = 10,
             const std::string& camera_id = "*");

    ~ZoneRule() override = default;

    std::string getId() const override { return id_; }
    std::string getName() const override { return name_; }
    std::string getType() const override { return "zone_intrusion"; }
    std::string getCameraId() const override { return camera_id_; }
    bool isEnabled() const override { return enabled_; }
    void setEnabled(bool enabled) override { enabled_ = enabled; }

    std::optional<Alert> evaluate(const std::vector<TrackedObject>& objects) override;

private:
    bool isPointInPolygon(const cv::Point2f& pt) const;

    std::string id_;
    std::string name_;
    std::vector<std::string> target_classes_;
    std::vector<cv::Point2f> polygon_;
    std::string severity_;
    std::string message_;
    int cooldown_sec_{10};
    std::string camera_id_{"*"};
    bool enabled_{true};

    std::chrono::steady_clock::time_point last_alert_time_;
};

} // namespace edge_ai

