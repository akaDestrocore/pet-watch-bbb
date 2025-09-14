#!/bin/sh
set -e

DTBO_NAME="BB-UART1-00A0"
DTBO_PATH="/lib/firmware/overlays/${DTBO_NAME}.dtbo"

if [ -w /sys/devices/platform/bone_capemgr/slots ]; then
    if grep -q ${DTBO_NAME} /sys/devices/platform/bone_capemgr/slots 2>/dev/null; then
        echo "Overlay ${DTBO_NAME} already present in manager"
    else
        if [ -f "${DTBO_PATH}" ]; then
            echo "${DTBO_NAME}" > /sys/devices/platform/bone_capemgr/slots || true
            sleep 0.5
        fi
    fi
fi

if [ -d /sys/devices/firmware ]; then
    if [ -w /sys/devices/firmware/overlays ]; then
        if [ -f "${DTBO_PATH}" ]; then
            echo "${DTBO_PATH}" > /sys/devices/firmware/overlays || true
            sleep 0.5
        fi
    fi
fi

if command -v config-pin >/dev/null 2>&1; then
    config-pin P9_24 uart || true
    config-pin P9_26 uart || true
fi

for i in $(seq 1 10); do
    if [ -c /dev/ttyS1 ]; then
        stty -F /dev/ttyS1 460800 raw -clocal -echo || true
        echo "UART1 configured at 460800 baud"
        break
    fi
    sleep 1
done

if [ ! -c /dev/video0 ]; then
    echo "Warning: /dev/video0 not available!"
fi
