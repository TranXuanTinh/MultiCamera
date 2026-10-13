#pragma once

#include "app/types.h"
#include <functional>
#include <vector>
#include <string>

namespace edge_ai { namespace hal {

/**
 * @brief Abstract Hardware Abstraction Layer for NVIDIA DeepStream SDK multi-camera pipeline.
 *
 * Encapsulates multi-source ingestion (CSI, RTSP, USB, file), nvstreammux batching,
 * nvinfer (Primary GIE), nvtracker, and metadata extraction via GStreamer pad probes.
 */
class IDeepStreamHAL {
public:
    virtual ~IDeepStreamHAL() = default;

    using MetadataCallback = std::function<void(const PipelineResult&)>;

    virtual bool initialize(const DeepStreamConfig& ds_config,
                            const std::vector<CameraSourceConfig>& cameras) = 0;
    virtual bool start() = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;

    virtual void setMetadataCallback(MetadataCallback cb) = 0;

    virtual float getStreamFps(int source_id) const = 0;
    virtual size_t getActiveSourceCount() const = 0;

    // Optional synchronous stepping for unit tests and single-step processing
    virtual bool stepFrame(PipelineResult& out_result) = 0;
};

}} // namespace edge_ai::hal

