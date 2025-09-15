SUMMARY = "config-pin utility"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "https://raw.githubusercontent.com/beagleboard/bb.org-overlays/master/tools/beaglebone-universal-io/config-pin"
SRC_URI[sha256sum] = "351f7c8b180f70e1efd86005536115d327ecd367753e1857824b002149cb62a8"

S = "${WORKDIR}"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${S}/config-pin ${D}${bindir}/
    sed -i 's|#!/bin/dash|#!/bin/sh|g' ${D}${bindir}/config-pin
}

FILES:${PN} = "${bindir}/config-pin"
