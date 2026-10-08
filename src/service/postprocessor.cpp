#include "service/postprocessor.h"
#include <fstream>
#include <algorithm>
#include <cmath>

namespace edge_ai {

PostProcessor::PostProcessor(float conf_thresh, float nms_thresh, const std::string& class_file)
    : conf_thresh_(conf_thresh), nms_thresh_(nms_thresh) {
    if (!class_file.empty()) {
        loadClassNames(class_file);
    }
}

bool PostProcessor::loadClassNames(const std::string& class_file) {
    std::ifstream file(class_file);
    if (!file.is_open()) return false;

    class_names_.clear();
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            class_names_.push_back(line);
        }
    }
    return !class_names_.empty();
}

std::vector<Detection> PostProcessor::decode(const float* raw_tensor_output,
                                            const PreprocessMeta& meta,
                                            int num_classes,
                                            int num_anchors) {
    std::vector<Detection> candidates;
    if (!raw_tensor_output) return candidates;

    // Tensor shape is [1, 4 + num_classes, num_anchors]
    // Layout: row 0: cx, row 1: cy, row 2: w, row 3: h, rows 4..83: class probabilities
    for (int a = 0; a < num_anchors; ++a) {
        float max_score = -1.0f;
        int max_class_id = -1;

        for (int c = 0; c < num_classes; ++c) {
            float score = raw_tensor_output[(4 + c) * num_anchors + a];
            if (score > max_score) {
                max_score = score;
                max_class_id = c;
            }
        }

        if (max_score >= conf_thresh_) {
            float cx = raw_tensor_output[0 * num_anchors + a];
            float cy = raw_tensor_output[1 * num_anchors + a];
            float w  = raw_tensor_output[2 * num_anchors + a];
            float h  = raw_tensor_output[3 * num_anchors + a];

            // Rescale back to original unpadded frame coordinates
            float orig_cx = (cx - meta.pad_x) / meta.scale;
            float orig_cy = (cy - meta.pad_y) / meta.scale;
            float orig_w  = w / meta.scale;
            float orig_h  = h / meta.scale;

            float x1 = orig_cx - orig_w * 0.5f;
            float y1 = orig_cy - orig_h * 0.5f;

            // Clip boundaries
            x1 = std::max(0.0f, std::min(x1, static_cast<float>(meta.orig_width)));
            y1 = std::max(0.0f, std::min(y1, static_cast<float>(meta.orig_height)));
            orig_w = std::min(orig_w, static_cast<float>(meta.orig_width) - x1);
            orig_h = std::min(orig_h, static_cast<float>(meta.orig_height) - y1);

            Detection det;
            det.bbox = {x1, y1, orig_w, orig_h};
            det.confidence = max_score;
            det.class_id = max_class_id;
            det.class_name = (max_class_id < static_cast<int>(class_names_.size())) ?
                             class_names_[max_class_id] : std::to_string(max_class_id);

            candidates.push_back(std::move(det));
        }
    }

    return nms(candidates);
}

std::vector<Detection> PostProcessor::nms(std::vector<Detection>& candidates) {
    if (candidates.empty()) return {};

    // Sort descending by confidence
    std::sort(candidates.begin(), candidates.end(),
              [](const Detection& a, const Detection& b) {
                  return a.confidence > b.confidence;
              });

    std::vector<Detection> kept;
    std::vector<bool> suppressed(candidates.size(), false);

    for (size_t i = 0; i < candidates.size(); ++i) {
        if (suppressed[i]) continue;

        kept.push_back(candidates[i]);

        for (size_t j = i + 1; j < candidates.size(); ++j) {
            if (suppressed[j]) continue;

            // Only suppress between identical classes (class-aware NMS)
            if (candidates[i].class_id == candidates[j].class_id) {
                float iou = candidates[i].bbox.iou(candidates[j].bbox);
                if (iou > nms_thresh_) {
                    suppressed[j] = true;
                }
            }
        }
    }

    return kept;
}

} // namespace edge_ai

