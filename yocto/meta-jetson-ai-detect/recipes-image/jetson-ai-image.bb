SUMMARY = "Minimal Production Image for Jetson Nano AI Edge Node"
DESCRIPTION = "Target image for Jetson Nano 4GB containing NVIDIA drivers, \
               CUDA, TensorRT, and edge-ai-manager daemon."

IMAGE_FEATURES += "ssh-server-openssh"

IMAGE_INSTALL = " \
    packagegroup-core-boot \
    packagegroup-core-full-cmdline \
    \
    kernel-modules \
    tegra-libraries \
    cuda-toolkit-10-2 \
    cuda-libraries-10-2 \
    cudnn \
    tensorrt \
    tensorrt-plugins \
    \
    opencv \
    gstreamer1.0 \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-nvarguscamerasrc \
    gstreamer1.0-plugins-nvvidconv \
    gstreamer1.0-plugins-nvvideo4linux2 \
    \
    edge-ai-manager \
    \
    systemd \
    networkmanager \
    openssh \
    htop \
    v4l-utils \
"

IMAGE_LINGUAS = ""
NO_RECOMMENDATIONS = "1"

inherit core-image

