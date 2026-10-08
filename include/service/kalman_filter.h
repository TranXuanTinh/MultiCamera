#pragma once

#include "app/types.h"
#include <opencv2/video/tracking.hpp>

namespace edge_ai {

// Kalman Filter for Bounding Box tracking: state is [x, y, a, h, vx, vy, va, vh]
// where a = aspect ratio (w/h), h = height
class KalmanBoxFilter {
public:
    KalmanBoxFilter();
    ~KalmanBoxFilter() = default;

    void init(const BBox& bbox);
    BBox predict();
    void update(const BBox& bbox);

    BBox getCurrentBox() const;
    void getVelocity(float& vx, float& vy) const;

private:
    cv::KalmanFilter kf_;
    BBox current_box_;
};

} // namespace edge_ai

