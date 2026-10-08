#include "service/kalman_filter.h"

namespace edge_ai {

KalmanBoxFilter::KalmanBoxFilter() {
    kf_.init(8, 4, 0);

    // State transition matrix (F)
    kf_.transitionMatrix = cv::Mat::eye(8, 8, CV_32F);
    for (int i = 0; i < 4; ++i) {
        kf_.transitionMatrix.at<float>(i, i + 4) = 1.0f; // dt = 1
    }

    // Measurement matrix (H)
    kf_.measurementMatrix = cv::Mat::zeros(4, 8, CV_32F);
    for (int i = 0; i < 4; ++i) {
        kf_.measurementMatrix.at<float>(i, i) = 1.0f;
    }

    // Measurement noise covariance (R)
    cv::setIdentity(kf_.measurementNoiseCov, cv::Scalar::all(1e-1));

    // Process noise covariance (Q)
    cv::setIdentity(kf_.processNoiseCov, cv::Scalar::all(1e-2));
    for (int i = 4; i < 8; ++i) {
        kf_.processNoiseCov.at<float>(i, i) = 1e-1;
    }

    // Error covariance post (P)
    cv::setIdentity(kf_.errorCovPost, cv::Scalar::all(1.0));
}

void KalmanBoxFilter::init(const BBox& bbox) {
    current_box_ = bbox;

    float cx = bbox.centerX();
    float cy = bbox.centerY();
    float h  = std::max(1.0f, bbox.h);
    float a  = bbox.w / h;

    kf_.statePost.at<float>(0) = cx;
    kf_.statePost.at<float>(1) = cy;
    kf_.statePost.at<float>(2) = a;
    kf_.statePost.at<float>(3) = h;
    kf_.statePost.at<float>(4) = 0.0f;
    kf_.statePost.at<float>(5) = 0.0f;
    kf_.statePost.at<float>(6) = 0.0f;
    kf_.statePost.at<float>(7) = 0.0f;
}

BBox KalmanBoxFilter::predict() {
    cv::Mat p = kf_.predict();

    float cx = p.at<float>(0);
    float cy = p.at<float>(1);
    float a  = p.at<float>(2);
    float h  = std::max(1.0f, p.at<float>(3));
    float w  = std::max(1.0f, a * h);

    current_box_.x = cx - w * 0.5f;
    current_box_.y = cy - h * 0.5f;
    current_box_.w = w;
    current_box_.h = h;

    return current_box_;
}

void KalmanBoxFilter::update(const BBox& bbox) {
    current_box_ = bbox;

    cv::Mat measurement(4, 1, CV_32F);
    measurement.at<float>(0) = bbox.centerX();
    measurement.at<float>(1) = bbox.centerY();
    measurement.at<float>(2) = bbox.w / std::max(1.0f, bbox.h);
    measurement.at<float>(3) = std::max(1.0f, bbox.h);

    kf_.correct(measurement);
}

BBox KalmanBoxFilter::getCurrentBox() const {
    return current_box_;
}

void KalmanBoxFilter::getVelocity(float& vx, float& vy) const {
    vx = kf_.statePost.at<float>(4);
    vy = kf_.statePost.at<float>(5);
}

} // namespace edge_ai

