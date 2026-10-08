#pragma once

#include "hal/i_network_hal.h"
#include <iostream>
#include <map>
#include <vector>

namespace edge_ai { namespace hal {

class MockNetworkHAL : public INetworkHAL {
public:
    MockNetworkHAL() = default;
    ~MockNetworkHAL() override = default;

    bool connect(const NetworkConfig& /*config*/) override {
        connected_ = true;
        return true;
    }

    void disconnect() override {
        connected_ = false;
    }

    bool isConnected() const override {
        return connected_;
    }

    bool publish(const std::string& topic,
                 const std::string& payload,
                 int /*qos*/ = 1) override {
        if (!connected_) return false;
        published_messages_[topic].push_back(payload);
        return true;
    }

    bool subscribe(const std::string& /*topic*/,
                   MessageCallback /*callback*/,
                   int /*qos*/ = 1) override {
        return connected_;
    }

    void setConnectionLostCallback(ConnectionLostCallback cb) override {
        conn_lost_cb_ = std::move(cb);
    }

    // Helper for verification during testing
    size_t getPublishedCount(const std::string& topic) const {
        auto it = published_messages_.find(topic);
        return (it != published_messages_.end()) ? it->second.size() : 0;
    }

private:
    bool connected_{false};
    ConnectionLostCallback conn_lost_cb_;
    std::map<std::string, std::vector<std::string>> published_messages_;
};

}} // namespace edge_ai::hal

