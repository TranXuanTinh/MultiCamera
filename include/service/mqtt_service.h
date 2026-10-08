#pragma once

#include "app/types.h"
#include "hal/i_network_hal.h"
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

namespace edge_ai {

class MqttService {
public:
    explicit MqttService(hal::INetworkHAL& network_hal);
    ~MqttService();

    bool connect(const MqttConfig& config);
    void disconnect();
    bool isConnected() const;

    bool publishDetections(const PipelineResult& result, const std::string& device_id);
    bool publishAlert(const Alert& alert, const std::string& device_id);
    bool publishHealth(const HealthStatus& health, const std::string& device_id);

    size_t getOfflineQueueSize() const;

private:
    void enqueueOffline(const std::string& topic, const std::string& payload, int qos);
    void drainOfflineQueue();
    void onConnectionLost(const std::string& cause);

    std::string serializeDetectionJson(const PipelineResult& result, const std::string& device_id);
    std::string serializeAlertJson(const Alert& alert, const std::string& device_id);
    std::string serializeHealthJson(const HealthStatus& health, const std::string& device_id);

    hal::INetworkHAL& hal_;
    MqttConfig config_;

    struct QueuedMsg {
        std::string topic;
        std::string payload;
        int qos{1};
    };

    std::queue<QueuedMsg> offline_queue_;
    mutable std::mutex queue_mtx_;

    std::chrono::steady_clock::time_point last_publish_time_;
};

} // namespace edge_ai

