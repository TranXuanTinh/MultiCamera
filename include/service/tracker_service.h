#pragma once

#include "app/types.h"
#include "service/kalman_filter.h"
#include <vector>

namespace edge_ai {

struct TrackInternal {
    int id{0};
    KalmanBoxFilter kf;
    Detection last_det;
    TrackState state{TrackState::New};
    int hits{0};
    int age{0};
    int time_since_update{0};
};

class TrackerService {
public:
    explicit TrackerService(const TrackerConfig& config);
    ~TrackerService() = default;

    std::vector<TrackedObject> update(const std::vector<Detection>& detections);
    void reset();

    size_t getActiveTracksCount() const { return tracks_.size(); }

private:
    TrackerConfig config_;
    int next_id_{1};
    std::vector<TrackInternal> tracks_;

    std::vector<std::vector<float>> computeCostMatrix(
        const std::vector<TrackInternal>& trks,
        const std::vector<Detection>& dets);
};

} // namespace edge_ai

