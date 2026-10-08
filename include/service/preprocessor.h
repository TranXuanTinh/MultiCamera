#pragma once

#include "app/types.h"
#include <opencv2/core.hpp>
#include <vector>

namespace edge_ai {

struct PreprocessMeta {
    float scale{1.0f};
    float pad_x{0.0f};
    float pad_y{0.0f};
    int orig_width{0};
    int orig_height{0};
};

class Preprocessor {
public:
    explicit Preprocessor(int input_size = 640);
    ~Preprocessor() = default;

    // Transforms input image into contiguous CHW float array, writes metadata
    bool process(const cv::Mat& src_bgr, std::vector<float>& dst_chw, PreprocessMeta& meta);

    // Transforms input image directly into FP16 half-precision buffer
    bool processFP16(const cv::Mat& src_bgr, std::vector<uint16_t>& dst_chw_fp16, PreprocessMeta& meta);

private:
    int input_size_{640};
};

} // namespace edge_ai

