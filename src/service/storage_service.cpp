#include "service/storage_service.h"
#include <sstream>
#include <iostream>

namespace edge_ai {

StorageService::StorageService(hal::IStorageHAL& storage_hal)
    : hal_(storage_hal) {
}

StorageService::~StorageService() {
    shutdown();
}

bool StorageService::initialize(const StorageConfig& config) {
    config_ = config;
    if (!config_.enabled) return true;

    if (!hal_.open(config_.db_path)) {
        return false;
    }

    createTables();

    running_ = true;
    writer_thread_ = std::thread(&StorageService::writerLoop, this);
    return true;
}

void StorageService::shutdown() {
    if (!running_) return;
    running_ = false;
    cv_.notify_all();

    if (writer_thread_.joinable()) {
        writer_thread_.join();
    }

    flushQueue();
    hal_.close();
}

void StorageService::createTables() {
    hal_.execute(
        "CREATE TABLE IF NOT EXISTS detections ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  camera_id TEXT NOT NULL DEFAULT 'cam_0',"
        "  frame_id INTEGER NOT NULL,"
        "  track_id INTEGER,"
        "  class_name TEXT NOT NULL,"
        "  class_id INTEGER NOT NULL,"
        "  confidence REAL NOT NULL,"
        "  bbox_x REAL NOT NULL,"
        "  bbox_y REAL NOT NULL,"
        "  bbox_w REAL NOT NULL,"
        "  bbox_h REAL NOT NULL"
        ");"
    );

    hal_.execute(
        "CREATE TABLE IF NOT EXISTS alerts ("
        "  id TEXT PRIMARY KEY,"
        "  camera_id TEXT NOT NULL DEFAULT 'cam_0',"
        "  rule_id TEXT NOT NULL,"
        "  rule_name TEXT NOT NULL,"
        "  severity TEXT NOT NULL,"
        "  timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  message TEXT NOT NULL,"
        "  track_id INTEGER,"
        "  class_name TEXT"
        ");"
    );

    hal_.execute("CREATE INDEX IF NOT EXISTS idx_det_timestamp ON detections(timestamp);");
    hal_.execute("CREATE INDEX IF NOT EXISTS idx_det_track ON detections(track_id);");
    hal_.execute("CREATE INDEX IF NOT EXISTS idx_det_camera ON detections(camera_id);");
}

void StorageService::recordDetection(uint64_t frame_id, const TrackedObject& obj) {
    recordDetection(obj.camera_id.empty() ? "cam_0" : obj.camera_id, frame_id, obj);
}

void StorageService::recordDetection(const std::string& camera_id, uint64_t frame_id, const TrackedObject& obj) {
    if (!config_.enabled) return;

    DetRecord rec;
    rec.camera_id = camera_id;
    rec.frame_id = frame_id;
    rec.track_id = obj.track_id;
    rec.class_name = obj.det.class_name;
    rec.class_id = obj.det.class_id;
    rec.conf = obj.det.confidence;
    rec.bbox = obj.det.bbox;

    {
        std::lock_guard<std::mutex> lock(queue_mtx_);
        write_queue_.push(std::move(rec));
    }
}

void StorageService::recordAlert(const Alert& alert) {
    if (!config_.enabled) return;

    std::string cam_id = alert.camera_id.empty() ? "cam_0" : alert.camera_id;
    std::ostringstream ss;
    ss << "INSERT INTO alerts (id, camera_id, rule_id, rule_name, severity, message, track_id, class_name) VALUES ("
       << "'" << alert.id << "', "
       << "'" << cam_id << "', "
       << "'" << alert.rule_id << "', "
       << "'" << alert.rule_name << "', "
       << "'" << alert.severity << "', "
       << "'" << alert.message << "', "
       << alert.trigger.track_id << ", "
       << "'" << alert.trigger.det.class_name << "');";

    hal_.execute(ss.str());
}

void StorageService::writerLoop() {
    while (running_) {
        std::unique_lock<std::mutex> lock(queue_mtx_);
        cv_.wait_for(lock, std::chrono::seconds(config_.flush_interval_sec), [this]() {
            return !running_ || write_queue_.size() >= 50;
        });

        lock.unlock();
        flushQueue();
    }
}

void StorageService::flushQueue() {
    std::vector<DetRecord> batch;
    {
        std::lock_guard<std::mutex> lock(queue_mtx_);
        if (write_queue_.empty()) return;

        batch.reserve(write_queue_.size());
        while (!write_queue_.empty()) {
            batch.push_back(std::move(write_queue_.front()));
            write_queue_.pop();
        }
    }

    if (batch.empty()) return;

    hal_.beginTransaction();
    for (const DetRecord& rec : batch) {
        std::ostringstream ss;
        ss << "INSERT INTO detections (camera_id, frame_id, track_id, class_name, class_id, confidence, bbox_x, bbox_y, bbox_w, bbox_h) VALUES ("
           << "'" << rec.camera_id << "', "
           << rec.frame_id << ", "
           << rec.track_id << ", '"
           << rec.class_name << "', "
           << rec.class_id << ", "
           << rec.conf << ", "
           << rec.bbox.x << ", "
           << rec.bbox.y << ", "
           << rec.bbox.w << ", "
           << rec.bbox.h << ");";
        hal_.execute(ss.str());
    }
    hal_.commitTransaction();

    total_written_ += batch.size();
    if (total_written_ % 1000 == 0) {
        enforceRingBuffer();
    }
}

void StorageService::enforceRingBuffer() {
    hal::SqlResultSet res = hal_.query("SELECT COUNT(*) AS cnt FROM detections;");
    if (!res.empty()) {
        try {
            int count = std::stoi(res[0].at("cnt"));
            if (count > config_.max_records) {
                // Delete oldest 10,000 records
                hal_.execute("DELETE FROM detections WHERE id IN (SELECT id FROM detections ORDER BY id ASC LIMIT 10000);");
            }
        } catch (...) {}
    }
}

} // namespace edge_ai

