#pragma once

#include "hal/i_watchdog_hal.h"

namespace edge_ai { namespace hal {

class MockWatchdogHAL : public IWatchdogHAL {
public:
    MockWatchdogHAL() = default;
    ~MockWatchdogHAL() override = default;

    bool open(const std::string& /*device_node*/, int /*timeout_sec*/) override {
        opened_ = true;
        return true;
    }

    void kick() override {
        ++kick_count_;
    }

    void close() override {
        opened_ = false;
    }

    bool isOpen() const override {
        return opened_;
    }

    uint64_t getKickCount() const { return kick_count_; }

private:
    bool opened_{false};
    uint64_t kick_count_{0};
};

}} // namespace edge_ai::hal

