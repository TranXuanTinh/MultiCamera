#pragma once

#include "app/types.h"
#include <string>
#include <vector>
#include <optional>

namespace edge_ai {

class IRule {
public:
    virtual ~IRule() = default;

    virtual std::string getId() const = 0;
    virtual std::string getName() const = 0;
    virtual std::string getType() const = 0;
    virtual std::string getCameraId() const = 0;
    virtual bool isEnabled() const = 0;
    virtual void setEnabled(bool enabled) = 0;

    virtual std::optional<Alert> evaluate(const std::vector<TrackedObject>& objects) = 0;
};

} // namespace edge_ai

