#pragma once

#include <string>

namespace edge_ai { namespace hal {

class IWatchdogHAL {
public:
    virtual ~IWatchdogHAL() = default;

    virtual bool open(const std::string& device_node, int timeout_sec) = 0;
    virtual void kick() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
};

}} // namespace edge_ai::hal

