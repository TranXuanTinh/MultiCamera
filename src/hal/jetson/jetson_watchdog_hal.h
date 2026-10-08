#pragma once

#include "hal/i_watchdog_hal.h"
#include <string>

namespace edge_ai { namespace hal {

class JetsonWatchdogHAL : public IWatchdogHAL {
public:
    JetsonWatchdogHAL();
    ~JetsonWatchdogHAL() override;

    bool open(const std::string& device_node, int timeout_sec) override;
    void kick() override;
    void close() override;
    bool isOpen() const override;

private:
    int fd_{-1};
};

}} // namespace edge_ai::hal

