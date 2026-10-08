# Low-Level Design Document (LLD)
## Project: Embedded AI Object Detection on Jetson Nano 4GB (Yocto Linux)

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
    float area() const { return w * h; }

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

### 1.2 Tracked Object State
```cpp
enum class TrackState : uint8_t { New = 0, Confirmed = 1, Lost = 2, Removed = 3 };

struct TrackedObject {
    int track_id{-1};
    Detection det;
    TrackState state{TrackState::New};
    float vx{0.0f};      // Horizontal velocity (pixels/frame)
    float vy{0.0f};      // Vertical velocity (pixels/frame)
    int age{0};          // Total frames since track creation
    int hits{0};         // Total frames successfully matched
    int time_since_update{0}; // Frames since last detection association
};
```

---

## 2. Mathematical Algorithms

### 2.1 Letterbox Preprocessing & Box Rescaling
To maintain the aspect ratio of a $1280 \times 720$ frame when resizing to the model's $640 \times 640$ input:
$$\text{scale} = \min\left(\frac{640}{1280}, \frac{640}{720}\right) = \min(0.5, 0.888) = 0.5$$
$$\text{new\_w} = 1280 \times 0.5 = 640, \quad \text{new\_h} = 720 \times 0.5 = 360$$
$$\text{pad\_x} = \frac{640 - 640}{2} = 0, \quad \text{pad\_y} = \frac{640 - 360}{2} = 140$$

**Inverse Rescaling Formula (Postprocessing)**:
For any bounding box $(x_{model}, y_{model}, w_{model}, h_{model})$ produced by YOLOv8:
$$x_{orig} = \frac{x_{model} - \text{pad\_x}}{\text{scale}}, \quad y_{orig} = \frac{y_{model} - \text{pad\_y}}{\text{scale}}$$
$$w_{orig} = \frac{w_{model}}{\text{scale}}, \quad h_{orig} = \frac{h_{model}}{\text{scale}}$$

### 2.2 YOLOv8 Output Decoding & Fast NMS
* Raw tensor shape: $[1, 84, 8400]$ (where $84 = 4 \text{ box coords} + 80 \text{ classes}$, and $8400 \text{ anchor points}$).
* Transpose on host or GPU kernel: $[8400, 84]$.
* For each anchor $i \in [0, 8399]$:
  1. Find maximum score $\text{max\_score} = \max_{c \in [0, 79]} \text{scores}[c]$.
  2. If $\text{max\_score} < 0.45$, discard immediately.
  3. Decode box: $(x_c, y_c, w, h) \to (x = x_c - w/2, y = y_c - h/2, w, h)$.
  4. Perform greedy Non-Maximum Suppression (IoU threshold $0.50$).

### 2.3 Ray-Casting Polygon Point-in-Polygon (Zone Rule)
To test if point $(x, y)$ is inside a polygon $P = [v_0, v_1, \dots, v_{n-1}]$:
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

---

## 3. SQLite Local Storage Schema

```sql
CREATE TABLE IF NOT EXISTS detections (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT NOT NULL,
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
    rule_id TEXT NOT NULL,
    rule_name TEXT NOT NULL,
    severity TEXT NOT NULL,
    timestamp TEXT NOT NULL,
    message TEXT NOT NULL,
    track_id INTEGER,
    class_name TEXT
);

CREATE INDEX IF NOT EXISTS idx_det_timestamp ON detections(timestamp);
CREATE INDEX IF NOT EXISTS idx_det_track ON detections(track_id);
```

### Ring-Buffer Auto-Purge Policy:
When total rows in `detections` exceed $100,000$:
```sql
DELETE FROM detections WHERE id IN (
    SELECT id FROM detections ORDER BY id ASC LIMIT 10000
);
```

---

## 4. Hardware Watchdog ioctl Protocol

Interaction with Linux kernel driver `/dev/watchdog`:
```cpp
// Open watchdog device with 60 second timeout
int fd = open("/dev/watchdog", O_WRONLY);
int timeout = 60;
ioctl(fd, WDIOC_SETTIMEOUT, &timeout);

// In HealthService thread, kick every 30 seconds:
ioctl(fd, WDIOC_KEEPALIVE, 0);

// Graceful close: write 'V' before close to prevent emergency reboot
write(fd, "V", 1);
close(fd);
```

