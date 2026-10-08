#include "service/tracker_service.h"
#include "service/hungarian.h"
#include <algorithm>

namespace edge_ai {

TrackerService::TrackerService(const TrackerConfig& config)
    : config_(config) {
}

void TrackerService::reset() {
    tracks_.clear();
    next_id_ = 1;
}

std::vector<std::vector<float>> TrackerService::computeCostMatrix(
    const std::vector<TrackInternal>& trks,
    const std::vector<Detection>& dets) {
    if (trks.empty() || dets.empty()) return {};

    std::vector<std::vector<float>> cost(trks.size(), std::vector<float>(dets.size(), 1.0f));

    for (size_t t = 0; t < trks.size(); ++t) {
        BBox pred_box = trks[t].kf.getCurrentBox();
        for (size_t d = 0; d < dets.size(); ++d) {
            // Cost is 1.0 - IoU (matching classes preferred)
            if (trks[t].last_det.class_id == dets[d].class_id) {
                float iou = pred_box.iou(dets[d].bbox);
                cost[t][d] = 1.0f - iou;
            } else {
                cost[t][d] = 1.0f; // Penalize different classes
            }
        }
    }
    return cost;
}

std::vector<TrackedObject> TrackerService::update(const std::vector<Detection>& detections) {
    // 1. Predict all current tracks with Kalman filter
    for (auto& trk : tracks_) {
        trk.kf.predict();
        trk.age++;
        trk.time_since_update++;
    }

    // 2. Separate detections into high-score and low-score (ByteTrack approach)
    std::vector<Detection> det_first;
    for (const auto& det : detections) {
        if (det.confidence >= 0.5f) {
            det_first.push_back(det);
        }
    }

    // 3. Associate high-score detections with tracks
    auto cost_mat = computeCostMatrix(tracks_, det_first);
    auto matches = HungarianAlgorithm::solve(cost_mat);

    std::vector<bool> track_matched(tracks_.size(), false);
    std::vector<bool> det_matched(det_first.size(), false);

    for (const auto& p : matches) {
        int t = p.first;
        int d = p.second;
        // Check IoU threshold
        if (cost_mat[t][d] <= (1.0f - config_.iou_thresh)) {
            tracks_[t].kf.update(det_first[d].bbox);
            tracks_[t].last_det = det_first[d];
            tracks_[t].hits++;
            tracks_[t].time_since_update = 0;
            if (tracks_[t].hits >= config_.min_hits) {
                tracks_[t].state = TrackState::Confirmed;
            }
            track_matched[t] = true;
            det_matched[d] = true;
        }
    }

    // 4. Create new tracks for unmatched high-confidence detections
    for (size_t d = 0; d < det_first.size(); ++d) {
        if (!det_matched[d]) {
            TrackInternal new_trk;
            new_trk.id = next_id_++;
            new_trk.kf.init(det_first[d].bbox);
            new_trk.last_det = det_first[d];
            new_trk.state = (config_.min_hits <= 1) ? TrackState::Confirmed : TrackState::New;
            new_trk.hits = 1;
            new_trk.age = 1;
            new_trk.time_since_update = 0;
            tracks_.push_back(std::move(new_trk));
        }
    }

    // 5. Update lost status and remove stale tracks
    for (size_t t = 0; t < tracks_.size(); ++t) {
        if (!track_matched[t] && tracks_[t].time_since_update > 0) {
            tracks_[t].state = TrackState::Lost;
        }
    }

    tracks_.erase(
        std::remove_if(tracks_.begin(), tracks_.end(),
                       [this](const TrackInternal& trk) {
                           return trk.time_since_update > config_.max_age;
                       }),
        tracks_.end()
    );

    // 6. Format output tracked objects
    std::vector<TrackedObject> result;
    result.reserve(tracks_.size());

    for (const auto& trk : tracks_) {
        if (trk.state == TrackState::Confirmed) {
            TrackedObject obj;
            obj.track_id = trk.id;
            obj.det = trk.last_det;
            obj.det.bbox = trk.kf.getCurrentBox();
            obj.state = trk.state;
            trk.kf.getVelocity(obj.vx, obj.vy);
            obj.age = trk.age;
            obj.hits = trk.hits;
            obj.time_since_update = trk.time_since_update;
            result.push_back(obj);
        }
    }

    return result;
}

} // namespace edge_ai

