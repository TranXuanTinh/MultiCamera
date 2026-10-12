#include "jetson_network_hal.h"
#include <iostream>

namespace edge_ai { namespace hal {

#if defined(ENABLE_PAHO_MQTT)
void JetsonNetworkHAL::onConnectionLost(void* context, char* cause) {
    JetsonNetworkHAL* self = static_cast<JetsonNetworkHAL*>(context);
    self->connected_ = false;
    if (self->conn_lost_cb_) {
        self->conn_lost_cb_(cause ? std::string(cause) : "Unknown cause");
    }
}

int JetsonNetworkHAL::onMessageArrived(void* context, char* topicName, int /*topicLen*/, MQTTClient_message* message) {
    JetsonNetworkHAL* self = static_cast<JetsonNetworkHAL*>(context);
    if (self->msg_cb_) {
        std::string payload(static_cast<char*>(message->payload), message->payloadlen);
        self->msg_cb_(std::string(topicName), payload);
    }
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}
#endif

JetsonNetworkHAL::JetsonNetworkHAL() = default;

JetsonNetworkHAL::~JetsonNetworkHAL() {
    disconnect();
}

bool JetsonNetworkHAL::connect(const NetworkConfig& config) {
    std::lock_guard<std::mutex> lock(mtx_);
#if defined(ENABLE_PAHO_MQTT)
    if (connected_) return true;

    int rc = MQTTClient_create(&client_, config.broker_address.c_str(), config.client_id.c_str(),
                               MQTTCLIENT_PERSISTENCE_NONE, nullptr);
    if (rc != MQTTCLIENT_SUCCESS) {
        return false;
    }

    MQTTClient_setCallbacks(client_, this, onConnectionLost, onMessageArrived, nullptr);

    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = config.keep_alive_sec;
    conn_opts.cleansession = 1;

    if (!config.username.empty()) {
        conn_opts.username = config.username.c_str();
        conn_opts.password = config.password.c_str();
    }

    rc = MQTTClient_connect(client_, &conn_opts);
    if (rc != MQTTCLIENT_SUCCESS) {
        MQTTClient_destroy(&client_);
        client_ = nullptr;
        return false;
    }

    connected_ = true;
    return true;
#else
    (void)config;
    connected_ = true;
    return true;
#endif
}

void JetsonNetworkHAL::disconnect() {
    std::lock_guard<std::mutex> lock(mtx_);
#if defined(ENABLE_PAHO_MQTT)
    if (client_ && connected_) {
        MQTTClient_disconnect(client_, 1000);
        MQTTClient_destroy(&client_);
        client_ = nullptr;
    }
#endif
    connected_ = false;
}

bool JetsonNetworkHAL::isConnected() const {
    return connected_;
}

bool JetsonNetworkHAL::publish(const std::string& topic,
                              const std::string& payload,
                              int qos) {
    if (!connected_) return false;

#if defined(ENABLE_PAHO_MQTT)
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = const_cast<char*>(payload.data());
    pubmsg.payloadlen = static_cast<int>(payload.size());
    pubmsg.qos = qos;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;
    int rc = MQTTClient_publishMessage(client_, topic.c_str(), &pubmsg, &token);
    return (rc == MQTTCLIENT_SUCCESS);
#else
    (void)topic;
    (void)payload;
    (void)qos;
    return true;
#endif
}

bool JetsonNetworkHAL::subscribe(const std::string& topic,
                                MessageCallback callback,
                                int qos) {
    if (!connected_) return false;
    msg_cb_ = std::move(callback);

#if defined(ENABLE_PAHO_MQTT)
    int rc = MQTTClient_subscribe(client_, topic.c_str(), qos);
    return (rc == MQTTCLIENT_SUCCESS);
#else
    (void)topic;
    (void)qos;
    return true;
#endif
}

void JetsonNetworkHAL::setConnectionLostCallback(ConnectionLostCallback callback) {
    conn_lost_cb_ = std::move(callback);
}

}} // namespace edge_ai::hal

