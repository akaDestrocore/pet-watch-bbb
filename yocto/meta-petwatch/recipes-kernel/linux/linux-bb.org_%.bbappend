FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += " \
    file://enable-overlay.cfg \
    file://0001-enable-UART1.patch \
"

KERNEL_CONFIG_FRAGMENTS += " \
    ${WORKDIR}/enable-overlay.cfg \
"