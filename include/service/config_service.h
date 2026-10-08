#pragma once

#include "app/types.h"
#include <string>

namespace edge_ai {

class ConfigService {
public:
    ConfigService() = default;
    ~ConfigService() = default;

    bool loadFromFile(const std::string& filepath);

    const CameraConfig& getCameraConfig() const { return camera_cfg_; }
    const ModelConfig& getModelConfig() const { return model_cfg_; }
    const TrackerConfig& getTrackerConfig() const { return tracker_cfg_; }
    const MqttConfig& getMqttConfig() const { return mqtt_cfg_; }
    const StorageConfig& getStorageConfig() const { return storage_cfg_; }
    const HealthConfig& getHealthConfig() const { return health_cfg_; }

    std::string getDeviceId() const { return device_id_; }
    std::string getRulesConfigFile() const { return rules_config_file_; }

private:
    std::string device_id_{"jetson-nano-001"};
    std::string rules_config_file_{"config/rules.yaml"};

    CameraConfig camera_cfg_;
    ModelConfig model_cfg_;
    TrackerConfig tracker_cfg_;
    MqttConfig mqtt_cfg_;
    StorageConfig storage_cfg_;
    HealthConfig health_cfg_;
};

} // namespace edge_ai

