#include "jetson_deepstream_hal.h"
#include <iostream>
#include <sstream>
#include <chrono>

#if defined(ENABLE_DEEPSTREAM)
#include <nvbufsurface.h>
#endif

namespace edge_ai { namespace hal {

JetsonDeepStreamHAL::JetsonDeepStreamHAL() = default;

JetsonDeepStreamHAL::~JetsonDeepStreamHAL() {
    stop();
}

bool JetsonDeepStreamHAL::initialize(const DeepStreamConfig& ds_config,
                                    const std::vector<CameraSourceConfig>& cameras) {
    ds_config_ = ds_config;
    cameras_ = cameras;
    source_id_to_cam_id_.clear();

    for (size_t i = 0; i < cameras_.size(); ++i) {
        source_id_to_cam_id_[static_cast<int>(i)] = cameras_[i].id;
    }

#if defined(ENABLE_DEEPSTREAM)
    gst_init(nullptr, nullptr);

    pipeline_ = gst_pipeline_new("deepstream-multicamera-pipeline");
    if (!pipeline_) {
        std::cerr << "[DeepStreamHAL] Failed to create GStreamer pipeline\n";
        return false;
    }

    stream_muxer_ = gst_element_factory_make("nvstreammux", "stream-muxer");
    if (!stream_muxer_) {
        std::cerr << "[DeepStreamHAL] Failed to create nvstreammux element\n";
        return false;
    }
    gst_bin_add(GST_BIN(pipeline_), stream_muxer_);

    g_object_set(G_OBJECT(stream_muxer_),
                 "batch-size", ds_config_.batch_size,
                 "batched-push-timeout", ds_config_.batched_push_timeout_us,
                 "width", ds_config_.muxer_width,
                 "height", ds_config_.muxer_height,
                 "live-source", TRUE,
                 "nvbuf-memory-type", 0,
                 nullptr);

    // Build source bins for each camera
    for (size_t i = 0; i < cameras_.size(); ++i) {
        const CameraSourceConfig& cam = cameras_[i];
        if (!cam.enabled) continue;

        std::string src_bin_name = "source-bin-" + std::to_string(i);
        GstElement* src_elem = nullptr;

        if (cam.type == "csi") {
            src_elem = gst_element_factory_make("nvarguscamerasrc", src_bin_name.c_str());
            if (src_elem) {
                g_object_set(G_OBJECT(src_elem),
                             "sensor-id", cam.sensor_id,
                             "bufapi-version", TRUE,
                             nullptr);
            }
        } else if (cam.type == "usb") {
            src_elem = gst_element_factory_make("v4l2src", src_bin_name.c_str());
            if (src_elem) {
                g_object_set(G_OBJECT(src_elem), "device", cam.uri.c_str(), nullptr);
            }
        } else if (cam.type == "rtsp" || cam.type == "file") {
            src_elem = gst_element_factory_make("nvurisrcbin", src_bin_name.c_str());
            if (src_elem) {
                g_object_set(G_OBJECT(src_elem),
                             "uri", cam.uri.c_str(),
                             "rtsp-reconnect-interval", cam.reconnect_interval_sec,
                             nullptr);
            }
        }

        if (!src_elem) {
            std::cerr << "[DeepStreamHAL] Failed to create source element for camera: " << cam.id << "\n";
            continue;
        }

        gst_bin_add(GST_BIN(pipeline_), src_elem);

        // Link source element pad to nvstreammux sink_%u pad
        std::string pad_name = "sink_" + std::to_string(i);
        GstPad* muxer_sink_pad = gst_element_get_request_pad(stream_muxer_, pad_name.c_str());
        GstPad* src_pad = gst_element_get_static_pad(src_elem, "src");

        if (src_pad && muxer_sink_pad) {
            gst_pad_link(src_pad, muxer_sink_pad);
            gst_object_unref(src_pad);
            gst_object_unref(muxer_sink_pad);
        }
    }

    // Primary GIE (nvinfer)
    pgie_ = gst_element_factory_make("nvinfer", "primary-nvinference-engine");
    if (!pgie_) {
        std::cerr << "[DeepStreamHAL] Failed to create nvinfer element\n";
        return false;
    }
    g_object_set(G_OBJECT(pgie_), "config-file-path", ds_config_.pgie_config_file.c_str(), nullptr);
    gst_bin_add(GST_BIN(pipeline_), pgie_);

    // Multi-stream Tracker (nvtracker)
    tracker_ = gst_element_factory_make("nvtracker", "multi-stream-tracker");
    if (!tracker_) {
        std::cerr << "[DeepStreamHAL] Failed to create nvtracker element\n";
        return false;
    }
    g_object_set(G_OBJECT(tracker_),
                 "ll-config-file", ds_config_.tracker_config_file.c_str(),
                 "enable-batch-process", TRUE,
                 nullptr);
    gst_bin_add(GST_BIN(pipeline_), tracker_);

    // Multi-stream Tiler (nvmultistreamtiler)
    tiler_ = gst_element_factory_make("nvmultistreamtiler", "multi-stream-tiler");
    if (tiler_) {
        g_object_set(G_OBJECT(tiler_),
                     "rows", ds_config_.tiler_rows,
                     "columns", ds_config_.tiler_cols,
                     "width", ds_config_.tiler_width,
                     "height", ds_config_.tiler_height,
                     nullptr);
        gst_bin_add(GST_BIN(pipeline_), tiler_);
    }

    // Video Converter
    nvvidconv_ = gst_element_factory_make("nvvideoconvert", "nvvideo-converter");
    if (nvvidconv_) {
        gst_bin_add(GST_BIN(pipeline_), nvvidconv_);
    }

    // On-Screen Display (nvdsosd)
    osd_ = gst_element_factory_make("nvdsosd", "nv-onscreendisplay");
    if (osd_) {
        g_object_set(G_OBJECT(osd_), "process-mode", 0, "display-text", TRUE, nullptr);
        gst_bin_add(GST_BIN(pipeline_), osd_);
    }

    // Sink (fakesink or display)
    const char* sink_plugin = ds_config_.sink_type.empty() ? "fakesink" : ds_config_.sink_type.c_str();
    sink_ = gst_element_factory_make(sink_plugin, "video-sink");
    if (!sink_) {
        sink_ = gst_element_factory_make("fakesink", "video-sink");
    }
    g_object_set(G_OBJECT(sink_), "sync", FALSE, nullptr);
    gst_bin_add(GST_BIN(pipeline_), sink_);

    // Link pipeline downstream: stream_muxer -> pgie -> tracker -> tiler -> nvvidconv -> osd -> sink
    if (!gst_element_link(stream_muxer_, pgie_)) {
        std::cerr << "[DeepStreamHAL] Failed to link stream_muxer to pgie\n";
        return false;
    }

    if (!gst_element_link(pgie_, tracker_)) {
        std::cerr << "[DeepStreamHAL] Failed to link pgie to tracker\n";
        return false;
    }

    GstElement* last_elem = tracker_;
    if (tiler_) {
        gst_element_link(last_elem, tiler_);
        last_elem = tiler_;
    }
    if (nvvidconv_) {
        gst_element_link(last_elem, nvvidconv_);
        last_elem = nvvidconv_;
    }
    if (osd_) {
        gst_element_link(last_elem, osd_);
        last_elem = osd_;
    }
    gst_element_link(last_elem, sink_);

    // Attach Pad Probe on tracker source pad (extracts detections and track IDs)
    GstPad* tracker_src_pad = gst_element_get_static_pad(tracker_, "src");
    if (tracker_src_pad) {
        gst_pad_add_probe(tracker_src_pad, GST_PAD_PROBE_TYPE_BUFFER,
                          &JetsonDeepStreamHAL::padProbeCallback, this, nullptr);
        gst_object_unref(tracker_src_pad);
    }

    // Setup Bus Watch for error/eos monitoring
    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
    bus_watch_id_ = gst_bus_add_watch(bus, &JetsonDeepStreamHAL::busCallback, this);
    gst_object_unref(bus);

#endif

    initialized_ = true;
    return true;
}

bool JetsonDeepStreamHAL::start() {
    if (!initialized_) return false;
    if (running_) return true;

#if defined(ENABLE_DEEPSTREAM)
    if (pipeline_) {
        GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            std::cerr << "[DeepStreamHAL] Failed to set pipeline to PLAYING\n";
            return false;
        }
    }
#endif

    running_ = true;
    paused_ = false;
    return true;
}

void JetsonDeepStreamHAL::pause() {
    paused_ = true;
#if defined(ENABLE_DEEPSTREAM)
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_PAUSED);
    }
#endif
}

void JetsonDeepStreamHAL::resume() {
    paused_ = false;
#if defined(ENABLE_DEEPSTREAM)
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    }
#endif
}

void JetsonDeepStreamHAL::stop() {
    running_ = false;
    paused_ = false;

#if defined(ENABLE_DEEPSTREAM)
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }
    if (bus_watch_id_ > 0) {
        g_source_remove(bus_watch_id_);
        bus_watch_id_ = 0;
    }
#endif
}

bool JetsonDeepStreamHAL::isRunning() const {
    return running_;
}

void JetsonDeepStreamHAL::setMetadataCallback(MetadataCallback cb) {
    std::lock_guard<std::mutex> lock(cb_mtx_);
    cb_ = std::move(cb);
}

float JetsonDeepStreamHAL::getStreamFps(int source_id) const {
    (void)source_id;
    return 30.0f;
}

size_t JetsonDeepStreamHAL::getActiveSourceCount() const {
    return cameras_.size();
}

bool JetsonDeepStreamHAL::stepFrame(PipelineResult& out_result) {
    (void)out_result;
    return false;
}

#if defined(ENABLE_DEEPSTREAM)

GstPadProbeReturn JetsonDeepStreamHAL::padProbeCallback(GstPad* /*pad*/, GstPadProbeInfo* info, gpointer user_data) {
    JetsonDeepStreamHAL* self = static_cast<JetsonDeepStreamHAL*>(user_data);
    GstBuffer* buf = reinterpret_cast<GstBuffer*>(info->data);
    if (self && buf) {
        self->handlePadProbe(buf);
    }
    return GST_PAD_PROBE_OK;
}

gboolean JetsonDeepStreamHAL::busCallback(GstBus* /*bus*/, GstMessage* msg, gpointer user_data) {
    (void)user_data;
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError* err = nullptr;
            gchar* debug = nullptr;
            gst_message_parse_error(msg, &err, &debug);
            std::cerr << "[DeepStreamHAL Bus Error] " << (err ? err->message : "Unknown error")
                      << " | Debug: " << (debug ? debug : "") << std::endl;
            if (err) g_error_free(err);
            if (debug) g_free(debug);
            break;
        }
        case GST_MESSAGE_WARNING: {
            GError* err = nullptr;
            gchar* debug = nullptr;
            gst_message_parse_warning(msg, &err, &debug);
            std::cerr << "[DeepStreamHAL Bus Warning] " << (err ? err->message : "Warning") << std::endl;
            if (err) g_error_free(err);
            if (debug) g_free(debug);
            break;
        }
        case GST_MESSAGE_EOS: {
            std::cout << "[DeepStreamHAL Bus] End of stream reached.\n";
            break;
        }
        default:
            break;
    }
    return TRUE;
}

void JetsonDeepStreamHAL::handlePadProbe(GstBuffer* buf) {
    NvDsBatchMeta* batch_meta = gst_buffer_get_nvds_batch_meta(buf);
    if (!batch_meta) return;

    for (NvDsMetaList* l_frame = batch_meta->frame_meta_list; l_frame != nullptr; l_frame = l_frame->next) {
        NvDsFrameMeta* frame_meta = reinterpret_cast<NvDsFrameMeta*>(l_frame->data);
        if (!frame_meta) continue;

        int src_id = frame_meta->source_id;
        std::string cam_id = "cam_" + std::to_string(src_id);
        auto it = source_id_to_cam_id_.find(src_id);
        if (it != source_id_to_cam_id_.end()) {
            cam_id = it->second;
        }

        PipelineResult res;
        res.camera_id = cam_id;
        res.source_id = src_id;
        res.frame_id = frame_meta->frame_num;
        res.batch_size = batch_meta->num_frames_in_batch;
        res.fps = 30.0f;

        for (NvDsMetaList* l_obj = frame_meta->obj_meta_list; l_obj != nullptr; l_obj = l_obj->next) {
            NvDsObjectMeta* obj_meta = reinterpret_cast<NvDsObjectMeta*>(l_obj->data);
            if (!obj_meta) continue;

            TrackedObject tobj;
            tobj.camera_id = cam_id;
            tobj.source_id = src_id;
            tobj.track_id = static_cast<int>(obj_meta->object_id);
            tobj.state = TrackState::Confirmed;

            tobj.det.camera_id = cam_id;
            tobj.det.source_id = src_id;
            tobj.det.class_id = obj_meta->class_id;
            tobj.det.confidence = obj_meta->confidence;
            if (obj_meta->obj_label) {
                tobj.det.class_name = obj_meta->obj_label;
            }

            tobj.det.bbox.x = obj_meta->rect_params.left;
            tobj.det.bbox.y = obj_meta->rect_params.top;
            tobj.det.bbox.w = obj_meta->rect_params.width;
            tobj.det.bbox.h = obj_meta->rect_params.height;

            res.objects.push_back(tobj);
        }

        MetadataCallback local_cb;
        {
            std::lock_guard<std::mutex> lock(cb_mtx_);
            local_cb = cb_;
        }
        if (local_cb) {
            local_cb(res);
        }
    }
}

#endif

}} // namespace edge_ai::hal

