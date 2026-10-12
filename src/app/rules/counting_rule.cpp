#include "app/rules/counting_rule.h"
#include <sstream>

namespace edge_ai {

CountingRule::CountingRule(const std::string& id,
                           const std::string& name,
                           const std::vector<std::string>& target_classes,
                           cv::Point2f p1, cv::Point2f p2,
                           const std::string& direction,
                           const std::string& severity,
                           const std::string& message)
    : id_(id), name_(name), target_classes_(target_classes),
      p1_(p1), p2_(p2), direction_(direction),
      severity_(severity), message_(message) {
}

float CountingRule::crossProduct(cv::Point2f a, cv::Point2f b, cv::Point2f c) const {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

std::optional<Alert> CountingRule::evaluate(const std::vector<TrackedObject>& objects) {
    if (!enabled_) return std::nullopt;

    for (const TrackedObject& obj : objects) {
        bool class_matches = target_classes_.empty();
        for (const std::string& cls : target_classes_) {
            if (obj.det.class_name == cls) {
                class_matches = true;
                break;
            }
        }
        if (!class_matches) continue;

        cv::Point2f center(obj.det.bbox.centerX(), obj.det.bbox.centerY());
        float current_side = crossProduct(p1_, p2_, center);

        std::map<int, float>::iterator it = previous_side_.find(obj.track_id);
        if (it != previous_side_.end()) {
            float prev_side = it->second;

            // Check if side signed distance transitioned across zero (crossed line)
            if ((prev_side > 0.0f && current_side < 0.0f) || (prev_side < 0.0f && current_side > 0.0f)) {
                it->second = current_side;
                total_count_++;

                Alert a;
                std::ostringstream ss;
                ss << "count-" << id_ << "-" << obj.track_id << "-" << total_count_;
                a.id = ss.str();
                a.rule_id = id_;
                a.rule_name = name_;
                a.severity = severity_;

                std::ostringstream msg;
                msg << message_ << " (Total: " << total_count_ << ")";
                a.message = msg.str();
                a.trigger = obj;
                a.timestamp = std::chrono::system_clock::now();
                return a;
            }
        }
        previous_side_[obj.track_id] = current_side;
    }

    return std::nullopt;
}

} // namespace edge_ai

