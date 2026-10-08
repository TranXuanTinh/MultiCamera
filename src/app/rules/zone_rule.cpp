#include "app/rules/zone_rule.h"
#include <sstream>

namespace edge_ai {

ZoneRule::ZoneRule(const std::string& id,
                   const std::string& name,
                   const std::vector<std::string>& target_classes,
                   const std::vector<cv::Point2f>& polygon_points,
                   const std::string& severity,
                   const std::string& message,
                   int cooldown_sec)
    : id_(id), name_(name), target_classes_(target_classes),
      polygon_(polygon_points), severity_(severity),
      message_(message), cooldown_sec_(cooldown_sec) {
}

bool ZoneRule::isPointInPolygon(const cv::Point2f& pt) const {
    bool inside = false;
    size_t n = polygon_.size();
    if (n < 3) return false;

    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        if (((polygon_[i].y > pt.y) != (polygon_[j].y > pt.y)) &&
            (pt.x < (polygon_[j].x - polygon_[i].x) * (pt.y - polygon_[i].y) / (polygon_[j].y - polygon_[i].y) + polygon_[i].x)) {
            inside = !inside;
        }
    }
    return inside;
}

std::optional<Alert> ZoneRule::evaluate(const std::vector<TrackedObject>& objects) {
    if (!enabled_) return std::nullopt;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_alert_time_).count();
    if (elapsed < cooldown_sec_) {
        return std::nullopt; // Cooldown active
    }

    for (const auto& obj : objects) {
        // Class filter check
        bool class_matches = target_classes_.empty();
        for (const auto& cls : target_classes_) {
            if (obj.det.class_name == cls) {
                class_matches = true;
                break;
            }
        }
        if (!class_matches) continue;

        cv::Point2f center(obj.det.bbox.centerX(), obj.det.bbox.centerY());
        if (isPointInPolygon(center)) {
            last_alert_time_ = now;

            Alert a;
            std::ostringstream ss;
            ss << "alert-" << id_ << "-" << obj.track_id;
            a.id = ss.str();
            a.rule_id = id_;
            a.rule_name = name_;
            a.severity = severity_;
            a.message = message_;
            a.trigger = obj;
            a.timestamp = std::chrono::system_clock::now();
            return a;
        }
    }

    return std::nullopt;
}

} // namespace edge_ai

