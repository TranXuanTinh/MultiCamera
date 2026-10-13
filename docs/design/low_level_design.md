# Low-Level Design Document (LLD)
## Project: Multi-Camera Edge AI Video Analytics on Jetson with NVIDIA DeepStream SDK

---

## 1. Core Data Structures (`types.h`)

### 1.1 Coordinates & Bounding Box
```cpp
struct BBox {
    float x{0.0f};  // Top-left x in pixels
    float y{0.0f};  // Top-left y in pixels
    float w{0.0f};  // Box width in pixels
    float h{0.0f};  // Box height in pixels

    float centerX() const { return x + w * 0.5f; }
    float centerY() const { return y + h * 0.5f; }
    float area() const { return std::max(0.0f, w) * std::max(0.0f, h); }

    float iou(const BBox& o) const {
        float x1 = std::max(x, o.x);
        float y1 = std::max(y, o.y);
        float x2 = std::min(x + w, o.x + o.w);
        float y2 = std::min(y + h, o.y + o.h);
        float interArea = std::max(0.0f, x2 - x1) * std::max(0.0f, y2 - y1);
        float unionArea = area() + o.area() - interArea;
        return (unionArea <= 0.0f) ? 0.0f : (interArea / unionArea);
    }
};
```

### 1.2 Multi-Camera Object Detection & Tracking State
```cpp
struct Detection {
    BBox bbox;
    float confidence{0.0f};
    int class_id{-1};
    std::string class_name;
    std::string camera_id{"cam_0"};
    int source_id{0};
};

enum class TrackState : uint8_t { New = 0, Confirmed = 1, Lost = 2, Removed = 3 };

struct TrackedObject {
    int track_id{-1};
    Detection det;
    TrackState state{TrackState::New};
    float vx{0.0f};              // Horizontal velocity (px/frame)
    float vy{0.0f};              // Vertical velocity (px/frame)
    int age{0};                  // Total frames since track creation
    int hits{0};                 // Total frames matched
    int time_since_update{0};    // Frames since last association
    std::string camera_id{"cam_0"};
    int source_id{0};
};
```

### 1.3 Multi-Camera Pipeline Result & Alerts
```cpp
struct PipelineResult {
    uint64_t frame_id{0};
    std::string camera_id{"cam_0"};
    int source_id{0};
    uint32_t batch_size{1};
    float inference_ms{0.0f};
    float fps{0.0f};
    std::vector<TrackedObject> objects;
    std::vector<Alert> alerts;
};

struct Alert {
    std::string id;
    std::string camera_id{"cam_0"};
    int source_id{0};
    std::string rule_id;
    std::string rule_name;
    std::string severity;   // "info", "medium", "high", "critical"
    std::string message;
    TrackedObject trigger;
    std::chrono::system_clock::time_point timestamp;
};
```

---

## 2. DeepStream Metadata Traversal Specification

Inside the GStreamer Pad Probe callback (`GST_PAD_PROBE_TYPE_BUFFER`), zero-copy metadata traversal is performed via DeepStream C APIs:

```cpp
void JetsonDeepStreamHAL::handlePadProbe(GstBuffer* buf) {
    // 1. Retrieve top-level batch metadata
    NvDsBatchMeta* batch_meta = gst_buffer_get_nvds_batch_meta(buf);
    if (!batch_meta) return;

    // 2. Iterate across frames in the current batch
    for (NvDsMetaList* l_frame = batch_meta->frame_meta_list; l_frame != nullptr; l_frame = l_frame->next) {
        NvDsFrameMeta* frame_meta = reinterpret_cast<NvDsFrameMeta*>(l_frame->data);
        if (!frame_meta) continue;

        int src_id = frame_meta->source_id;
        std::string cam_id = source_id_to_cam_id_[src_id];

        PipelineResult res;
        res.camera_id  = cam_id;
        res.source_id  = src_id;
        res.frame_id   = frame_meta->frame_num;
        res.batch_size = batch_meta->num_frames_in_batch;
        res.fps        = 30.0f;

        // 3. Iterate across detected & tracked objects within the frame
        for (NvDsMetaList* l_obj = frame_meta->obj_meta_list; l_obj != nullptr; l_obj = l_obj->next) {
            NvDsObjectMeta* obj_meta = reinterpret_cast<NvDsObjectMeta*>(l_obj->data);
            if (!obj_meta) continue;

            TrackedObject tobj;
            tobj.camera_id = cam_id;
            tobj.source_id = src_id;
            tobj.track_id  = static_cast<int>(obj_meta->object_id);
            tobj.state     = TrackState::Confirmed;

            tobj.det.camera_id  = cam_id;
            tobj.det.source_id  = src_id;
            tobj.det.class_id   = obj_meta->class_id;
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

        // 4. Invoke user callback (Thread-safe dispatch)
        if (cb_) {
            cb_(res);
        }
    }
}
```

---

## 3. Video Analytics Services Algorithms (Modular Path)

### 3.1 Preprocessor (Letterbox Transform)
To adapt arbitrary camera resolutions (e.g. $1280 \times 720$) into YOLOv8 input size ($640 \times 640$) without distortion:
1. Scale ratio $r = \min\left(\frac{640}{W}, \frac{640}{H}\right)$.
2. Scaled dimensions: $w_{\text{scaled}} = W \times r$, $h_{\text{scaled}} = H \times r$.
3. Offsets: $dx = (640 - w_{\text{scaled}}) / 2$, $dy = (640 - h_{\text{scaled}}) / 2$.
4. Normalization and CHW formatting:
   $$\text{tensor}[c, y, x] = \frac{\text{pixel}[y, x, c]}{255.0}$$

### 3.2 Postprocessor (YOLOv8 Output Tensor Decoding)
The YOLOv8 ONNX model outputs tensor shape $(1, 84, 8400)$:
* 84 rows = 4 bounding box coordinates $[cx, cy, w, h] + 80$ COCO class scores.
* 8400 columns = candidate anchor points.
Decoding logic:
1. For each candidate $j \in [0, 8399]$:
   * Find class score $S_{\max} = \max_{k \in [0, 79]} \text{score}[k, j]$ and class index $C$.
   * If $S_{\max} < \text{confidence\_threshold}$, reject.
2. Unletterbox bounding box:
   $$x = \frac{cx - w/2 - dx}{r}, \quad y = \frac{cy - h/2 - dy}{r}, \quad w' = \frac{w}{r}, \quad h' = \frac{h}{r}$$
3. Apply Non-Maximum Suppression (NMS) with IoU threshold $= 0.50$.

### 3.3 TrackerService (ByteTrack with Kalman Filter & Hungarian)
* **Kalman Filter**: Tracks 8D state vector $[x, y, a, h, \dot{x}, \dot{y}, \dot{a}, \dot{h}]^T$ where $a = w/h$ is aspect ratio.
* **Two-Stage Association**:
  * Stage 1: Associate confirmed tracks with high-confidence detections ($S \ge 0.5$) using Hungarian assignment on $1 - \text{IoU}$ cost matrix.
  * Stage 2: Associate remaining unconfirmed tracks with low-confidence detections ($0.1 \le S < 0.5$) to maintain tracks through partial occlusions.

---

## 4. Mathematical Domain Rules

### 4.1 Ray-Casting Polygon Point-in-Polygon (`ZoneRule`)
Determines whether point $(x, y)$ is enclosed within an arbitrary $n$-vertex polygon $P = [v_0, v_1, \dots, v_{n-1}]$:
```cpp
bool isPointInsidePolygon(const cv::Point2f& pt, const std::vector<cv::Point2f>& poly) {
    bool inside = false;
    size_t n = poly.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        if (((poly[i].y > pt.y) != (poly[j].y > pt.y)) &&
            (pt.x < (poly[j].x - poly[i].x) * (pt.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x)) {
            inside = !inside;
        }
    }
    return inside;
}
```

### 4.2 2D Vector Cross-Product (`CountingRule`)
Determines which side of directed segment $\vec{AB}$ point $C$ lies on:
$$\text{crossProduct}(A, B, C) = (B_x - A_x) \times (C_y - A_y) - (B_y - A_y) \times (C_x - A_x)$$
* If $\text{sign}(\text{crossProduct}_{t-1}) \neq \text{sign}(\text{crossProduct}_t)$, the object crossed the tripwire during the frame transition.

---

## 5. SQLite Multi-Camera Storage Schema

```sql
CREATE TABLE IF NOT EXISTS detections (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    camera_id TEXT NOT NULL DEFAULT 'cam_0',
    frame_id INTEGER NOT NULL,
    track_id INTEGER,
    class_name TEXT NOT NULL,
    class_id INTEGER NOT NULL,
    confidence REAL NOT NULL,
    bbox_x REAL NOT NULL,
    bbox_y REAL NOT NULL,
    bbox_w REAL NOT NULL,
    bbox_h REAL NOT NULL
);

CREATE TABLE IF NOT EXISTS alerts (
    id TEXT PRIMARY KEY,
    camera_id TEXT NOT NULL DEFAULT 'cam_0',
    rule_id TEXT NOT NULL,
    rule_name TEXT NOT NULL,
    severity TEXT NOT NULL,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    message TEXT NOT NULL,
    track_id INTEGER,
    class_name TEXT
);

CREATE INDEX IF NOT EXISTS idx_det_timestamp ON detections(timestamp);
CREATE INDEX IF NOT EXISTS idx_det_track ON detections(track_id);
CREATE INDEX IF NOT EXISTS idx_det_camera ON detections(camera_id);
```

### Ring Buffer Auto-Purge Policy:
When total rows in `detections` exceed $100,000$, delete oldest $10,000$ rows:
```sql
DELETE FROM detections WHERE id IN (
    SELECT id FROM detections ORDER BY id ASC LIMIT 10000
);
```

---

## 6. Hardware Watchdog ioctl Protocol (`IWatchdogHAL`)

```cpp
int fd = open("/dev/watchdog", O_WRONLY);
int timeout = 60;
ioctl(fd, WDIOC_SETTIMEOUT, &timeout);

// In HealthService worker thread (every 30 seconds):
ioctl(fd, WDIOC_KEEPALIVE, 0);

// Graceful close: write 'V' before close to prevent emergency board reboot
write(fd, "V", 1);
close(fd);
```
