#include "service/preprocessor.h"
#include <opencv2/imgproc.hpp>
#include <cmath>

namespace edge_ai {

Preprocessor::Preprocessor(int input_size)
    : input_size_(input_size) {
}

bool Preprocessor::process(const cv::Mat& src_bgr, std::vector<float>& dst_chw, PreprocessMeta& meta) {
    if (src_bgr.empty()) return false;

    meta.orig_width = src_bgr.cols;
    meta.orig_height = src_bgr.rows;

    // 1. Calculate letterbox scale & paddings
    float r = std::min(static_cast<float>(input_size_) / meta.orig_width,
                       static_cast<float>(input_size_) / meta.orig_height);

    int new_unpad_w = static_cast<int>(std::round(meta.orig_width * r));
    int new_unpad_h = static_cast<int>(std::round(meta.orig_height * r));

    meta.scale = r;
    meta.pad_x = (input_size_ - new_unpad_w) * 0.5f;
    meta.pad_y = (input_size_ - new_unpad_h) * 0.5f;

    // 2. Resize
    cv::Mat resized;
    if (meta.orig_width != new_unpad_w || meta.orig_height != new_unpad_h) {
        cv::resize(src_bgr, resized, cv::Size(new_unpad_w, new_unpad_h), 0, 0, cv::INTER_LINEAR);
    } else {
        resized = src_bgr;
    }

    // 3. Create canvas and pad with neutral gray (114)
    cv::Mat canvas(input_size_, input_size_, CV_8UC3, cv::Scalar(114, 114, 114));
    int top = static_cast<int>(std::round(meta.pad_y));
    int left = static_cast<int>(std::round(meta.pad_x));
    resized.copyTo(canvas(cv::Rect(left, top, new_unpad_w, new_unpad_h)));

    // 4. Color convert BGR -> RGB
    cv::Mat rgb;
    cv::cvtColor(canvas, rgb, cv::COLOR_BGR2RGB);

    // 5. Convert to float and normalize to [0, 1] in CHW order
    size_t channel_size = static_cast<size_t>(input_size_ * input_size_);
    dst_chw.resize(3 * channel_size);

    float* r_plane = dst_chw.data();
    float* g_plane = dst_chw.data() + channel_size;
    float* b_plane = dst_chw.data() + 2 * channel_size;

    for (int y = 0; y < input_size_; ++y) {
        const cv::Vec3b* row_ptr = rgb.ptr<cv::Vec3b>(y);
        int row_offset = y * input_size_;
        for (int x = 0; x < input_size_; ++x) {
            const cv::Vec3b& pixel = row_ptr[x];
            r_plane[row_offset + x] = pixel[0] * (1.0f / 255.0f);
            g_plane[row_offset + x] = pixel[1] * (1.0f / 255.0f);
            b_plane[row_offset + x] = pixel[2] * (1.0f / 255.0f);
        }
    }

    return true;
}

// Convert float to FP16 representation
static uint16_t floatToHalf(float f) {
    uint32_t x;
    std::memcpy(&x, &f, sizeof(float));
    uint32_t sign = (x >> 31) & 0x1;
    uint32_t exp = (x >> 23) & 0xFF;
    uint32_t mant = x & 0x7FFFFF;

    if (exp == 0) return static_cast<uint16_t>(sign << 15);
    if (exp == 0xFF) return static_cast<uint16_t>((sign << 15) | 0x7C00);

    int new_exp = static_cast<int>(exp) - 127 + 15;
    if (new_exp >= 31) return static_cast<uint16_t>((sign << 15) | 0x7C00);
    if (new_exp <= 0) return static_cast<uint16_t>(sign << 15);

    return static_cast<uint16_t>((sign << 15) | (new_exp << 10) | (mant >> 13));
}

bool Preprocessor::processFP16(const cv::Mat& src_bgr, std::vector<uint16_t>& dst_chw_fp16, PreprocessMeta& meta) {
    std::vector<float> float_buf;
    if (!process(src_bgr, float_buf, meta)) return false;

    dst_chw_fp16.resize(float_buf.size());
    for (size_t i = 0; i < float_buf.size(); ++i) {
        dst_chw_fp16[i] = floatToHalf(float_buf[i]);
    }
    return true;
}

} // namespace edge_ai

