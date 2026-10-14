#include "app/rules/zone_rule.h"
#include <sstream>

namespace edge_ai {

ZoneRule::ZoneRule(const std::string& id,
                   const std::string& name,
                   const std::vector<std::string>& target_classes,
                   const std::vector<cv::Point2f>& polygon_points,
                   const std::string& severity,
                   const std::string& message,
                   int cooldown_sec,
                   const std::string& camera_id)
    : id_(id), name_(name), target_classes_(target_classes),
      polygon_(polygon_points), severity_(severity),
      message_(message), cooldown_sec_(cooldown_sec),
      camera_id_(camera_id) {
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

    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

    for (const TrackedObject& obj : objects) {
        // Camera ID check: if rule specifies a camera, skip objects from other cameras
        if (!camera_id_.empty() && camera_id_ != "*" && obj.camera_id != camera_id_) {
            continue;
        }

        // Check cooldown per camera
        auto it = last_alert_time_per_cam_.find(obj.camera_id);
        if (it != last_alert_time_per_cam_.end()) {
            long long elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count();
            if (elapsed < cooldown_sec_) {
                continue; // Cooldown active for this specific camera
            }
        }

        // Class filter check
        bool class_matches = target_classes_.empty();
        for (const std::string& cls : target_classes_) {
            if (obj.det.class_name == cls) {
                class_matches = true;
                break;
            }
        }
        if (!class_matches) continue;

        cv::Point2f center(obj.det.bbox.centerX(), obj.det.bbox.centerY());
        if (isPointInPolygon(center)) {
            last_alert_time_per_cam_[obj.camera_id] = now;

            Alert a;
            std::ostringstream ss;
            ss << "alert-" << id_ << "-" << obj.camera_id << "-" << obj.track_id;
            a.id = ss.str();
            a.camera_id = obj.camera_id;
            a.source_id = obj.source_id;
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

void ZoneRule::resetCameraState(const std::string& camera_id) {
    last_alert_time_per_cam_.erase(camera_id);
}

} // namespace edge_ai

