#pragma once

#include "hal/i_network_hal.h"
#include <atomic>
#include <mutex>
#include <string>

#if defined(ENABLE_PAHO_MQTT)
#include <MQTTClient.h>
#endif

namespace edge_ai { namespace hal {

class JetsonNetworkHAL : public INetworkHAL {
public:
    JetsonNetworkHAL();
    ~JetsonNetworkHAL() override;

    bool connect(const NetworkConfig& config) override;
    void disconnect() override;
    bool isConnected() const override;

    bool publish(const std::string& topic,
                 const std::string& payload,
                 int qos = 1) override;

    virtual bool subscribe(const std::string& topic,
                           MessageCallback callback,
                           int qos = 1) override;

    void setConnectionLostCallback(ConnectionLostCallback callback) override;

private:
#if defined(ENABLE_PAHO_MQTT)
    MQTTClient client_{nullptr};
    static void onConnectionLost(void* context, char* cause);
    static int onMessageArrived(void* context, char* topicName, int topicLen, MQTTClient_message* message);
#endif

    std::atomic<bool> connected_{false};
    ConnectionLostCallback conn_lost_cb_;
    MessageCallback msg_cb_;
    std::mutex mtx_;
};

}} // namespace edge_ai::hal

