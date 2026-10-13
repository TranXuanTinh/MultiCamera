#pragma once

#include "hal/i_deepstream_hal.h"
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <map>

#if defined(ENABLE_DEEPSTREAM)
#include <gst/gst.h>
#include <gstnvdsmeta.h>
#endif

namespace edge_ai { namespace hal {

class JetsonDeepStreamHAL : public IDeepStreamHAL {
public:
    JetsonDeepStreamHAL();
    ~JetsonDeepStreamHAL() override;

    bool initialize(const DeepStreamConfig& ds_config,
                    const std::vector<CameraSourceConfig>& cameras) override;
    bool start() override;
    void pause() override;
    void resume() override;
    void stop() override;
    bool isRunning() const override;

    void setMetadataCallback(MetadataCallback cb) override;

    float getStreamFps(int source_id) const override;
    size_t getActiveSourceCount() const override;

    bool stepFrame(PipelineResult& out_result) override;

#if defined(ENABLE_DEEPSTREAM)
    static GstPadProbeReturn padProbeCallback(GstPad* pad, GstPadProbeInfo* info, gpointer user_data);
    static gboolean busCallback(GstBus* bus, GstMessage* msg, gpointer user_data);
    void handlePadProbe(GstBuffer* buf);
#endif

private:
    DeepStreamConfig ds_config_;
    std::vector<CameraSourceConfig> cameras_;
    std::map<int, std::string> source_id_to_cam_id_;

    std::atomic<bool> initialized_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};

    std::mutex cb_mtx_;
    MetadataCallback cb_;

#if defined(ENABLE_DEEPSTREAM)
    GstElement* pipeline_{nullptr};
    GstElement* stream_muxer_{nullptr};
    GstElement* pgie_{nullptr};
    GstElement* tracker_{nullptr};
    GstElement* tiler_{nullptr};
    GstElement* nvvidconv_{nullptr};
    GstElement* osd_{nullptr};
    GstElement* sink_{nullptr};
    guint bus_watch_id_{0};
    GstLoop* loop_{nullptr};
    std::thread loop_thread_;
#endif
};

}} // namespace edge_ai::hal

