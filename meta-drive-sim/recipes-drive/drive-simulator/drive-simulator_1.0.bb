SUMMARY = "VirtualDrive simulated industrial drive controller"
DESCRIPTION = "Software-simulated EtherCAT/PROFINET/EtherNet/IP drive with CiA 402 state machine"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=c106ee394d8256068e0ea44d87c3291c"

# Protocol libraries are optional — the binary builds with stubs when absent.
# List them as DEPENDS so Yocto builds them first if they exist.
DEPENDS = "soes p-net opener"

inherit cmake systemd

SRC_URI = " \
    file://LICENSE \
    file://CMakeLists.txt \
    file://src/ \
    file://test/ \
    file://drive-simulator.service \
    file://virtualdrive.gsdml \
"

S = "${WORKDIR}"

EXTRA_OECMAKE = " \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH \
"

SYSTEMD_SERVICE:${PN} = "drive-simulator.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_install:append() {
    install -d ${D}${bindir}
    install -m 0755 ${B}/drive-simulator ${D}${bindir}/drive-simulator

    install -d ${D}${systemd_unitdir}/system
    install -m 0644 ${WORKDIR}/drive-simulator.service \
        ${D}${systemd_unitdir}/system/drive-simulator.service

    install -d ${D}${datadir}/drive-simulator
    install -m 0644 ${WORKDIR}/virtualdrive.gsdml \
        ${D}${datadir}/drive-simulator/virtualdrive.gsdml
}

FILES:${PN} = " \
    ${bindir}/drive-simulator \
    ${systemd_unitdir}/system/drive-simulator.service \
    ${datadir}/drive-simulator/ \
"

RDEPENDS:${PN} = "libstdc++ libgcc"
