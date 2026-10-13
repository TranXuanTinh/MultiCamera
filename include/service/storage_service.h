#pragma once

#include "app/types.h"
#include "hal/i_storage_hal.h"
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

namespace edge_ai {

class StorageService {
public:
    explicit StorageService(hal::IStorageHAL& storage_hal);
    ~StorageService();

    bool initialize(const StorageConfig& config);
    void shutdown();

    void recordDetection(uint64_t frame_id, const TrackedObject& obj);
    void recordDetection(const std::string& camera_id, uint64_t frame_id, const TrackedObject& obj);
    void recordAlert(const Alert& alert);

private:
    void writerLoop();
    void flushQueue();
    void enforceRingBuffer();
    void createTables();

    hal::IStorageHAL& hal_;
    StorageConfig config_;

    struct DetRecord {
        std::string camera_id{"cam_0"};
        uint64_t frame_id{0};
        int track_id{0};
        std::string class_name;
        int class_id{0};
        float conf{0.0f};
        BBox bbox;
    };

    std::queue<DetRecord> write_queue_;
    std::mutex queue_mtx_;
    std::condition_variable cv_;

    std::atomic<bool> running_{false};
    std::thread writer_thread_;
    uint64_t total_written_{0};
};

} // namespace edge_ai

