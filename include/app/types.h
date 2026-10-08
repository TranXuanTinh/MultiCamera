#pragma once

#include <opencv2/core.hpp>
#include <chrono>
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <any>
#include <optional>
#include <functional>

namespace edge_ai {

// ============================================================================
// Core Geometric & Frame Types
// ============================================================================

struct BBox {
    float x{0.0f};  // Top-left x
    float y{0.0f};  // Top-left y
    float w{0.0f};  // Width
    float h{0.0f};  // Height

    float centerX() const { return x + w * 0.5f; }
    float centerY() const { return y + h * 0.5f; }
    float area() const { return std::max(0.0f, w) * std::max(0.0f, h); }

    float iou(const BBox& o) const {
        float x1 = std::max(x, o.x);
        float y1 = std::max(y, o.y);
        float x2 = std::min(x + w, o.x + o.w);
        float y2 = std::min(y + h, o.y + o.h);
        float interArea = std::max(0.0f, x2 - x1) * std::max(0.0f, y2 - y1);
        float unionArea = area() + o.area() - interArea;
        return (unionArea <= 0.0f) ? 0.0f : (interArea / unionArea);
    }
};

struct Frame {
    cv::Mat image;
    uint64_t frame_id{0};
    std::chrono::steady_clock::time_point timestamp;
    int width{0};
    int height{0};
};

struct Detection {
    BBox bbox;
    float confidence{0.0f};
    int class_id{-1};
    std::string class_name;
};

// ============================================================================
// Tracking Types
// ============================================================================

enum class TrackState : uint8_t {
    New = 0,
    Confirmed = 1,
    Lost = 2,
    Removed = 3
};

struct TrackedObject {
    int track_id{-1};
    Detection det;
    TrackState state{TrackState::New};
    float vx{0.0f};      // Horizontal velocity (px/frame)
    float vy{0.0f};      // Vertical velocity (px/frame)
    int age{0};          // Total frames since creation
    int hits{0};         // Consecutive matched detections
    int time_since_update{0};
};

// ============================================================================
// Alerts & Telemetry
// ============================================================================

struct Alert {
    std::string id;
    std::string rule_id;
    std::string rule_name;
    std::string severity;   // "info", "medium", "high", "critical"
    std::string message;
    TrackedObject trigger;
    std::chrono::system_clock::time_point timestamp;
};

struct PipelineResult {
    uint64_t frame_id{0};
    float inference_ms{0.0f};
    float fps{0.0f};
    std::vector<TrackedObject> objects;
    std::vector<Alert> alerts;
};

struct HealthStatus {
    float cpu_usage{0.0f};
    float cpu_temp{0.0f};
    float gpu_usage{0.0f};
    float gpu_temp{0.0f};
    uint64_t ram_used_mb{0};
    uint64_t ram_total_mb{0};
    float disk_usage_pct{0.0f};
    float avg_fps{0.0f};
    float avg_latency_ms{0.0f};
    std::string status{"healthy"}; // "healthy", "warning", "critical"
};

// ============================================================================
// Configuration Data Objects
// ============================================================================

struct CameraConfig {
    std::string type{"csi"};
    std::string device{"/dev/video0"};
    int width{1280};
    int height{720};
    int fps{30};
    int csi_sensor_id{0};
    int csi_flip_method{0};
    int reconnect_interval_sec{5};
};

struct ModelConfig {
    std::string onnx_path;
    std::string engine_path;
    std::string class_file;
    int input_size{640};
    float conf_thresh{0.45f};
    float nms_thresh{0.50f};
    std::string precision{"fp16"};
};

struct TrackerConfig {
    int max_age{30};
    int min_hits{3};
    float iou_thresh{0.3f};
};

struct MqttConfig {
    bool enabled{true};
    std::string broker{"tcp://127.0.0.1:1883"};
    std::string client_id{"jetson-nano-001"};
    std::string topic_detections{"edge/detections"};
    std::string topic_alerts{"edge/alerts"};
    std::string topic_health{"edge/health"};
    int qos{1};
    int publish_interval_ms{100};
    int offline_queue_size{500};
    std::string username;
    std::string password;
};

struct StorageConfig {
    bool enabled{true};
    std::string db_path{"/var/lib/jetson-ai-detect/detections.db"};
    int max_records{100000};
    int flush_interval_sec{5};
};

struct HealthConfig {
    int interval_sec{30};
    bool watchdog_enabled{true};
    std::string watchdog_device{"/dev/watchdog"};
    int watchdog_timeout_sec{60};
    float cpu_temp_warn{70.0f};
    float cpu_temp_critical{85.0f};
};

// ============================================================================
// Event Definitions
// ============================================================================

enum class EventType : uint8_t {
    TRACKED_OBJECTS = 1,
    ALERT           = 2,
    HEALTH_STATUS   = 3,
    CONFIG_CHANGED  = 4
};

struct Event {
    EventType type;
    std::chrono::system_clock::time_point timestamp;
    std::any data;

    template<typename T>
    const T& as() const {
        return std::any_cast<const T&>(data);
    }
};

using EventCallback = std::function<void(const Event&)>;
using SubscriptionId = uint64_t;

} // namespace edge_ai

