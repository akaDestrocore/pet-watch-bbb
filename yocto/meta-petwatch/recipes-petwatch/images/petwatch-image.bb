SUMMARY = "PetWatch System Image"
DESCRIPTION = "Minimal Linux image with PetWatch application"

inherit core-image

# Core packages
IMAGE_FEATURES += "ssh-server-openssh"

IMAGE_INSTALL:append = " \
    kernel-modules \
    kernel-devicetree \
    config-pin \
    systemd-serialgetty \
    openssh \
    python3 \
    python3-modules \
    python3-opencv \
    python3-pyserial \
    python3-numpy \
    v4l-utils \
    usbutils \
    petwatch \
    "

# Camera, UART and USB support
IMAGE_INSTALL:append = " \
    kernel-module-uvcvideo \
    kernel-module-usb-storage \
    "

# Debugging
EXTRA_IMAGE_FEATURES += "debug-tweaks"
