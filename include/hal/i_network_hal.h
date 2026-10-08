#pragma once

#include <string>
#include <functional>
#include <cstddef>

namespace edge_ai { namespace hal {

struct NetworkConfig {
    std::string broker_address; // e.g. "tcp://127.0.0.1:1883"
    std::string client_id;
    std::string username;
    std::string password;
    int keep_alive_sec{60};
};

using MessageCallback = std::function<void(const std::string& topic, const std::string& payload)>;
using ConnectionLostCallback = std::function<void(const std::string& reason)>;

class INetworkHAL {
public:
    virtual ~INetworkHAL() = default;

    virtual bool connect(const NetworkConfig& config) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;

    virtual bool publish(const std::string& topic,
                         const std::string& payload,
                         int qos = 1) = 0;

    virtual bool subscribe(const std::string& topic,
                           MessageCallback callback,
                           int qos = 1) = 0;

    virtual void setConnectionLostCallback(ConnectionLostCallback callback) = 0;
};

}} // namespace edge_ai::hal

