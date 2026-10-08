#pragma once

#include "app/types.h"
#include "service/preprocessor.h"
#include <vector>
#include <string>

namespace edge_ai {

class PostProcessor {
public:
    PostProcessor(float conf_thresh = 0.45f,
                  float nms_thresh = 0.50f,
                  const std::string& class_file = "");
    ~PostProcessor() = default;

    bool loadClassNames(const std::string& class_file);

    std::vector<Detection> decode(const float* raw_tensor_output,
                                  const PreprocessMeta& meta,
                                  int num_classes = 80,
                                  int num_anchors = 8400);

private:
    float conf_thresh_{0.45f};
    float nms_thresh_{0.50f};
    std::vector<std::string> class_names_;

    std::vector<Detection> nms(std::vector<Detection>& candidates);
};

} // namespace edge_ai

