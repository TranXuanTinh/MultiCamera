#include "jetson_watchdog_hal.h"
#include <iostream>

#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/watchdog.h>
#endif

namespace edge_ai { namespace hal {

JetsonWatchdogHAL::JetsonWatchdogHAL() = default;

JetsonWatchdogHAL::~JetsonWatchdogHAL() {
    close();
}

bool JetsonWatchdogHAL::open(const std::string& device_node, int timeout_sec) {
    close();

#if !defined(_WIN32)
    fd_ = ::open(device_node.c_str(), O_WRONLY);
    if (fd_ < 0) {
        return false;
    }

    if (timeout_sec > 0) {
        ioctl(fd_, WDIOC_SETTIMEOUT, &timeout_sec);
    }
    return true;
#else
    (void)device_node;
    (void)timeout_sec;
    fd_ = 100; // Simulated valid descriptor on Windows
    return true;
#endif
}

void JetsonWatchdogHAL::kick() {
#if !defined(_WIN32)
    if (fd_ >= 0) {
        ioctl(fd_, WDIOC_KEEPALIVE, 0);
    }
#endif
}

void JetsonWatchdogHAL::close() {
#if !defined(_WIN32)
    if (fd_ >= 0) {
        // Send Magic character 'V' to safely disarm the watchdog before closing
        ::write(fd_, "V", 1);
        ::close(fd_);
        fd_ = -1;
    }
#else
    fd_ = -1;
#endif
}

bool JetsonWatchdogHAL::isOpen() const {
    return (fd_ >= 0);
}

}} // namespace edge_ai::hal

