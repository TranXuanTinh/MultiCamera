#include "service/mqtt_service.h"
#include <sstream>
#include <iomanip>

namespace edge_ai {

static std::string getIsoTimestamp() {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

MqttService::MqttService(hal::INetworkHAL& network_hal)
    : hal_(network_hal) {
    hal_.setConnectionLostCallback([this](const std::string& cause) {
        onConnectionLost(cause);
    });
}

MqttService::~MqttService() {
    disconnect();
}

bool MqttService::connect(const MqttConfig& config) {
    config_ = config;
    if (!config_.enabled) return true;

    hal::NetworkConfig net_cfg;
    net_cfg.broker_address = config.broker;
    net_cfg.client_id = config.client_id;
    net_cfg.username = config.username;
    net_cfg.password = config.password;
    net_cfg.keep_alive_sec = 60;

    bool ok = hal_.connect(net_cfg);
    if (ok) {
        drainOfflineQueue();
    }
    return ok;
}

void MqttService::disconnect() {
    hal_.disconnect();
}

bool MqttService::isConnected() const {
    return hal_.isConnected();
}

void MqttService::onConnectionLost(const std::string& /*cause*/) {
    // Network HAL will automatically manage reconnection or Service can retry
}

void MqttService::enqueueOffline(const std::string& topic, const std::string& payload, int qos) {
    std::lock_guard<std::mutex> lock(queue_mtx_);
    if (offline_queue_.size() >= static_cast<size_t>(config_.offline_queue_size)) {
        offline_queue_.pop(); // Drop oldest message
    }
    offline_queue_.push({topic, payload, qos});
}

void MqttService::drainOfflineQueue() {
    std::lock_guard<std::mutex> lock(queue_mtx_);
    while (!offline_queue_.empty() && hal_.isConnected()) {
        const QueuedMsg& item = offline_queue_.front();
        if (hal_.publish(item.topic, item.payload, item.qos)) {
            offline_queue_.pop();
        } else {
            break;
        }
    }
}

size_t MqttService::getOfflineQueueSize() const {
    std::lock_guard<std::mutex> lock(queue_mtx_);
    return offline_queue_.size();
}

std::string MqttService::serializeDetectionJson(const PipelineResult& result, const std::string& device_id) {
    std::ostringstream ss;
    ss << "{"
       << "\"device_id\":\"" << device_id << "\","
       << "\"camera_id\":\"" << result.camera_id << "\","
       << "\"source_id\":" << result.source_id << ","
       << "\"timestamp\":\"" << getIsoTimestamp() << "\","
       << "\"frame_id\":" << result.frame_id << ","
       << "\"inference_ms\":" << std::fixed << std::setprecision(1) << result.inference_ms << ","
       << "\"fps\":" << std::setprecision(1) << result.fps << ","
       << "\"objects\":[";

    for (size_t i = 0; i < result.objects.size(); ++i) {
        const TrackedObject& obj = result.objects[i];
        if (i > 0) ss << ",";
        ss << "{"
           << "\"track_id\":" << obj.track_id << ","
           << "\"camera_id\":\"" << obj.camera_id << "\","
           << "\"class\":\"" << obj.det.class_name << "\","
           << "\"confidence\":" << std::setprecision(2) << obj.det.confidence << ","
           << "\"bbox\":{"
           << "\"x\":" << std::setprecision(1) << obj.det.bbox.x << ","
           << "\"y\":" << obj.det.bbox.y << ","
           << "\"w\":" << obj.det.bbox.w << ","
           << "\"h\":" << obj.det.bbox.h
           << "},"
           << "\"vx\":" << std::setprecision(2) << obj.vx << ","
           << "\"vy\":" << obj.vy
           << "}";
    }
    ss << "]}";
    return ss.str();
}

std::string MqttService::serializeAlertJson(const Alert& alert, const std::string& device_id) {
    std::ostringstream ss;
    ss << "{"
       << "\"device_id\":\"" << device_id << "\","
       << "\"camera_id\":\"" << alert.camera_id << "\","
       << "\"source_id\":" << alert.source_id << ","
       << "\"alert_id\":\"" << alert.id << "\","
       << "\"rule_id\":\"" << alert.rule_id << "\","
       << "\"rule_name\":\"" << alert.rule_name << "\","
       << "\"severity\":\"" << alert.severity << "\","
       << "\"timestamp\":\"" << getIsoTimestamp() << "\","
       << "\"message\":\"" << alert.message << "\","
       << "\"trigger_object\":{"
       << "\"track_id\":" << alert.trigger.track_id << ","
       << "\"camera_id\":\"" << alert.trigger.camera_id << "\","
       << "\"class\":\"" << alert.trigger.det.class_name << "\","
       << "\"confidence\":" << std::fixed << std::setprecision(2) << alert.trigger.det.confidence
       << "}}";
    return ss.str();
}

std::string MqttService::serializeHealthJson(const HealthStatus& health, const std::string& device_id) {
    std::ostringstream ss;
    ss << "{"
       << "\"device_id\":\"" << device_id << "\","
       << "\"timestamp\":\"" << getIsoTimestamp() << "\","
       << "\"status\":\"" << health.status << "\","
       << "\"cpu\":{\"usage_pct\":" << std::fixed << std::setprecision(1) << health.cpu_usage
       << ",\"temp_c\":" << health.cpu_temp << "},"
       << "\"gpu\":{\"usage_pct\":" << health.gpu_usage
       << ",\"temp_c\":" << health.gpu_temp << "},"
       << "\"memory\":{\"used_mb\":" << health.ram_used_mb
       << ",\"total_mb\":" << health.ram_total_mb << "},"
       << "\"disk\":{\"usage_pct\":" << health.disk_usage_pct << "},"
       << "\"performance\":{\"fps\":" << health.avg_fps
       << ",\"latency_ms\":" << health.avg_latency_ms << "}";

    if (!health.camera_metrics.empty()) {
        ss << ",\"cameras\":[";
        for (size_t i = 0; i < health.camera_metrics.size(); ++i) {
            const auto& cm = health.camera_metrics[i];
            if (i > 0) ss << ",";
            ss << "{"
               << "\"camera_id\":\"" << cm.camera_id << "\","
               << "\"source_id\":" << cm.source_id << ","
               << "\"input_fps\":" << cm.input_fps << ","
               << "\"processed_fps\":" << cm.processed_fps << ","
               << "\"dropped_frames\":" << cm.dropped_frames << ","
               << "\"last_pts\":" << cm.last_pts << ","
               << "\"decode_latency_ms\":" << cm.decode_latency_ms << ","
               << "\"queue_depth\":" << cm.result_queue_depth << ","
               << "\"active_objects\":" << cm.active_objects
               << "}";
        }
        ss << "]";
    }
    ss << "}";
    return ss.str();
}

bool MqttService::publishDetections(const PipelineResult& result, const std::string& device_id) {
    if (!config_.enabled) return true;

    // Rate limiting check
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_publish_time_).count();
    if (elapsed < config_.publish_interval_ms) {
        return true; // Skip to preserve bandwidth
    }
    last_publish_time_ = now;

    std::string payload = serializeDetectionJson(result, device_id);
    if (!hal_.isConnected()) {
        enqueueOffline(config_.topic_detections, payload, config_.qos);
        return false;
    }

    drainOfflineQueue();
    return hal_.publish(config_.topic_detections, payload, config_.qos);
}

bool MqttService::publishAlert(const Alert& alert, const std::string& device_id) {
    if (!config_.enabled) return true;

    std::string payload = serializeAlertJson(alert, device_id);
    if (!hal_.isConnected()) {
        enqueueOffline(config_.topic_alerts, payload, 2); // QoS 2 for critical alerts
        return false;
    }

    drainOfflineQueue();
    return hal_.publish(config_.topic_alerts, payload, 2);
}

bool MqttService::publishHealth(const HealthStatus& health, const std::string& device_id) {
    if (!config_.enabled) return true;

    std::string payload = serializeHealthJson(health, device_id);
    if (!hal_.isConnected()) {
        enqueueOffline(config_.topic_health, payload, 1);
        return false;
    }

    return hal_.publish(config_.topic_health, payload, 1);
}

} // namespace edge_ai

