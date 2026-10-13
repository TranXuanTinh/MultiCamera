#include "service/config_service.h"
#include <fstream>
#include <sstream>
#include <iostream>

#if defined(ENABLE_YAML_CPP)
#include <yaml-cpp/yaml.h>
#endif

namespace edge_ai {

bool ConfigService::loadFromFile(const std::string& filepath) {
#if defined(ENABLE_YAML_CPP)
    try {
        YAML::Node config = YAML::LoadFile(filepath);

        if (config["system"]) {
            if (config["system"]["device_id"]) {
                device_id_ = config["system"]["device_id"].as<std::string>();
            }
        }

        cameras_.clear();
        if (config["cameras"]) {
            for (const YAML::Node& node : config["cameras"]) {
                CameraSourceConfig cam;
                if (node["id"]) cam.id = node["id"].as<std::string>();
                if (node["name"]) cam.name = node["name"].as<std::string>();
                if (node["type"]) cam.type = node["type"].as<std::string>();
                if (node["uri"]) cam.uri = node["uri"].as<std::string>();
                else if (node["device"]) cam.uri = node["device"].as<std::string>();
                if (node["sensor_id"]) cam.sensor_id = node["sensor_id"].as<int>();
                if (node["flip_method"]) cam.flip_method = node["flip_method"].as<int>();
                if (node["width"]) cam.width = node["width"].as<int>();
                if (node["height"]) cam.height = node["height"].as<int>();
                if (node["fps"]) cam.fps = node["fps"].as<int>();
                if (node["enabled"]) cam.enabled = node["enabled"].as<bool>();
                if (node["reconnect_interval_sec"]) cam.reconnect_interval_sec = node["reconnect_interval_sec"].as<int>();
                cameras_.push_back(cam);
            }
        }

        if (config["camera"]) {
            YAML::Node c = config["camera"];
            if (c["type"]) camera_cfg_.type = c["type"].as<std::string>();
            if (c["device"]) camera_cfg_.device = c["device"].as<std::string>();
            if (c["width"]) camera_cfg_.width = c["width"].as<int>();
            if (c["height"]) camera_cfg_.height = c["height"].as<int>();
            if (c["fps"]) camera_cfg_.fps = c["fps"].as<int>();
            if (c["csi_sensor_id"]) camera_cfg_.csi_sensor_id = c["csi_sensor_id"].as<int>();
            if (c["csi_flip_method"]) camera_cfg_.csi_flip_method = c["csi_flip_method"].as<int>();

            if (cameras_.empty()) {
                CameraSourceConfig cam;
                cam.id = "cam_0";
                cam.name = "Camera 0";
                cam.type = camera_cfg_.type;
                cam.uri = camera_cfg_.device;
                cam.sensor_id = camera_cfg_.csi_sensor_id;
                cam.flip_method = camera_cfg_.csi_flip_method;
                cam.width = camera_cfg_.width;
                cam.height = camera_cfg_.height;
                cam.fps = camera_cfg_.fps;
                cameras_.push_back(cam);
            }
        }

        if (config["deepstream"]) {
            YAML::Node ds = config["deepstream"];
            if (ds["batch_size"]) ds_cfg_.batch_size = ds["batch_size"].as<int>();
            if (ds["batched_push_timeout_us"]) ds_cfg_.batched_push_timeout_us = ds["batched_push_timeout_us"].as<int>();
            if (ds["muxer_width"]) ds_cfg_.muxer_width = ds["muxer_width"].as<int>();
            if (ds["muxer_height"]) ds_cfg_.muxer_height = ds["muxer_height"].as<int>();
            if (ds["pgie_config"]) ds_cfg_.pgie_config_file = ds["pgie_config"].as<std::string>();
            if (ds["tracker_config"]) ds_cfg_.tracker_config_file = ds["tracker_config"].as<std::string>();
            if (ds["enable_tiler"]) ds_cfg_.enable_tiler = ds["enable_tiler"].as<bool>();
            if (ds["tiler_rows"]) ds_cfg_.tiler_rows = ds["tiler_rows"].as<int>();
            if (ds["tiler_cols"]) ds_cfg_.tiler_cols = ds["tiler_cols"].as<int>();
            if (ds["tiler_width"]) ds_cfg_.tiler_width = ds["tiler_width"].as<int>();
            if (ds["tiler_height"]) ds_cfg_.tiler_height = ds["tiler_height"].as<int>();
            if (ds["enable_osd"]) ds_cfg_.enable_osd = ds["enable_osd"].as<bool>();
            if (ds["sink_type"]) ds_cfg_.sink_type = ds["sink_type"].as<std::string>();
        }

        if (config["model"]) {
            YAML::Node m = config["model"];
            if (m["onnx_path"]) model_cfg_.onnx_path = m["onnx_path"].as<std::string>();
            if (m["engine_path"]) model_cfg_.engine_path = m["engine_path"].as<std::string>();
            if (m["class_file"]) model_cfg_.class_file = m["class_file"].as<std::string>();
            if (m["input_size"]) model_cfg_.input_size = m["input_size"].as<int>();
            if (m["confidence_threshold"]) model_cfg_.conf_thresh = m["confidence_threshold"].as<float>();
            if (m["nms_threshold"]) model_cfg_.nms_thresh = m["nms_threshold"].as<float>();
            if (m["precision"]) model_cfg_.precision = m["precision"].as<std::string>();
        }

        if (config["tracker"]) {
            YAML::Node t = config["tracker"];
            if (t["max_age"]) tracker_cfg_.max_age = t["max_age"].as<int>();
            if (t["min_hits"]) tracker_cfg_.min_hits = t["min_hits"].as<int>();
            if (t["iou_threshold"]) tracker_cfg_.iou_thresh = t["iou_threshold"].as<float>();
        }

        if (config["mqtt"]) {
            YAML::Node q = config["mqtt"];
            if (q["enabled"]) mqtt_cfg_.enabled = q["enabled"].as<bool>();
            if (q["broker"]) mqtt_cfg_.broker = q["broker"].as<std::string>();
            if (q["client_id"]) mqtt_cfg_.client_id = q["client_id"].as<std::string>();
            if (q["topic_detections"]) mqtt_cfg_.topic_detections = q["topic_detections"].as<std::string>();
            if (q["topic_alerts"]) mqtt_cfg_.topic_alerts = q["topic_alerts"].as<std::string>();
            if (q["topic_health"]) mqtt_cfg_.topic_health = q["topic_health"].as<std::string>();
            if (q["qos"]) mqtt_cfg_.qos = q["qos"].as<int>();
            if (q["publish_interval_ms"]) mqtt_cfg_.publish_interval_ms = q["publish_interval_ms"].as<int>();
        }

        if (config["storage"]) {
            YAML::Node s = config["storage"];
            if (s["enabled"]) storage_cfg_.enabled = s["enabled"].as<bool>();
            if (s["db_path"]) storage_cfg_.db_path = s["db_path"].as<std::string>();
            if (s["max_records"]) storage_cfg_.max_records = s["max_records"].as<int>();
            if (s["flush_interval_sec"]) storage_cfg_.flush_interval_sec = s["flush_interval_sec"].as<int>();
        }

        if (config["health"]) {
            YAML::Node h = config["health"];
            if (h["interval_sec"]) health_cfg_.interval_sec = h["interval_sec"].as<int>();
            if (h["watchdog_enabled"]) health_cfg_.watchdog_enabled = h["watchdog_enabled"].as<bool>();
            if (h["watchdog_device"]) health_cfg_.watchdog_device = h["watchdog_device"].as<std::string>();
            if (h["watchdog_timeout_sec"]) health_cfg_.watchdog_timeout_sec = h["watchdog_timeout_sec"].as<int>();
            if (h["temp_warn_cpu"]) health_cfg_.cpu_temp_warn = h["temp_warn_cpu"].as<float>();
            if (h["temp_critical_cpu"]) health_cfg_.cpu_temp_critical = h["temp_critical_cpu"].as<float>();
        }

        if (config["rules"] && config["rules"]["config_file"]) {
            rules_config_file_ = config["rules"]["config_file"].as<std::string>();
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ConfigService] Failed to load " << filepath << ": " << e.what() << std::endl;
        return false;
    }
#else
    // Default safe fallback parameters
    (void)filepath;
    model_cfg_.onnx_path = "models/yolov8n.onnx";
    model_cfg_.engine_path = "models/yolov8n_fp16.engine";
    model_cfg_.class_file = "models/coco_classes.txt";

    if (cameras_.empty()) {
        cameras_.push_back({"cam_01", "Camera 1 (CSI)", "csi", "/dev/video0", 0, 0, 1280, 720, 30, true, 5});
        cameras_.push_back({"cam_02", "Camera 2 (RTSP)", "rtsp", "rtsp://127.0.0.1:8554/live", 1, 0, 1280, 720, 30, true, 5});
    }
    ds_cfg_.batch_size = 2;
    return true;
#endif
}

} // namespace edge_ai

