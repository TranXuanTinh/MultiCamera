SUMMARY = "Edge AI Object Detection on Jetson Nano with TensorRT and Yocto"
DESCRIPTION = "Production C++17 application implementing 3-layer architecture \
               for YOLOv8 real-time detection, ByteTrack tracking, and MQTT telemetry."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://CMakeLists.txt \
    file://include/ \
    file://src/ \
    file://config/ \
    file://models/ \
    file://systemd/jetson-ai-detect.service \
"

S = "${WORKDIR}"

inherit cmake systemd pkgconfig

DEPENDS = " \
    cuda-toolkit-10-2 \
    cudnn \
    tensorrt \
    opencv \
    gstreamer1.0 \
    gstreamer1.0-plugins-base \
    paho-mqtt-c \
    sqlite3 \
    yaml-cpp \
"

RDEPENDS_${PN} = " \
    cuda-libraries-10-2 \
    cudnn \
    tensorrt \
    libopencv-core \
    libopencv-imgproc \
    libopencv-videoio \
    gstreamer1.0-plugins-nvarguscamerasrc \
    gstreamer1.0-plugins-nvvidconv \
    paho-mqtt-c \
    sqlite3 \
    yaml-cpp \
"

EXTRA_OECMAKE = " \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_JETSON_CUDA=ON \
    -DENABLE_PAHO_MQTT=ON \
    -DENABLE_SQLITE3=ON \
    -DENABLE_YAML_CPP=ON \
    -DBUILD_TESTS=OFF \
"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${B}/edge-ai-manager ${D}${bindir}/

    install -d ${D}/opt/jetson-ai-detect/config
    install -m 0644 ${S}/config/app_config.yaml ${D}/opt/jetson-ai-detect/config/
    install -m 0644 ${S}/config/rules.yaml ${D}/opt/jetson-ai-detect/config/

    install -d ${D}/opt/jetson-ai-detect/models
    install -m 0644 ${S}/models/coco_classes.txt ${D}/opt/jetson-ai-detect/models/

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${S}/systemd/jetson-ai-detect.service ${D}${systemd_system_unitdir}/

    install -d ${D}/var/log/jetson-ai-detect
    install -d ${D}/var/lib/jetson-ai-detect
}

SYSTEMD_SERVICE_${PN} = "jetson-ai-detect.service"
SYSTEMD_AUTO_ENABLE = "enable"

FILES_${PN} += " \
    ${bindir}/edge-ai-manager \
    /opt/jetson-ai-detect \
    /var/log/jetson-ai-detect \
    /var/lib/jetson-ai-detect \
"

COMPATIBLE_MACHINE = "jetson-nano"

