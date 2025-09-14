SUMMARY = "PetWatch Motion Detection System"
DESCRIPTION = "Automated pet detection system using OpenCV"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# Dependencies
DEPENDS = "python3 opencv python3-pyserial python3-numpy"
RDEPENDS:${PN} = "python3-core python3-pyserial python3-numpy"

# Source files
SRC_URI = "file://watch.py \
           file://petwatch.service \
           file://uart-config.sh"

S = "${WORKDIR}"

do_install() {
    # Python script
    install -d ${D}${bindir}
    install -m 0755 ${S}/watch.py ${D}${bindir}/petwatch

    # UART config
    install -m 0755 ${S}/uart-config.sh ${D}${bindir}/uart-config

    # Systemd service
    install -d ${D}${systemd_unitdir}/system
    install -m 0644 ${S}/petwatch.service ${D}${systemd_unitdir}/system/

    # Enable service
    install -d ${D}${sysconfdir}/systemd/system/multi-user.target.wants
    ln -sf ${systemd_unitdir}/system/petwatch.service \
        ${D}${sysconfdir}/systemd/system/multi-user.target.wants/
}

FILES:${PN} += "${systemd_unitdir}/system/petwatch.service \
                ${sysconfdir}/systemd/system/multi-user.target.wants/petwatch.service"

inherit systemd
SYSTEMD_SERVICE:${PN} = "petwatch.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

