SUMMARY = "SOES — Simple Open EtherCAT Slave stack"
DESCRIPTION = "EtherCAT slave stack implementing CiA 301 object dictionary and process \
data exchange. Provides ecat_slv_init/ecat_slv_poll API used by drive-simulator."
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://LICENSE;md5=b234ee4d69f5fce4486a80fdaf4a4263"

SRC_URI = "git://github.com/OpenEtherCATsociety/SOES.git;protocol=https;branch=master"
SRCREV = "${AUTOREV}"
PV = "1.0+git${SRCPV}"

S = "${WORKDIR}/git"

inherit cmake

# Build static libraries only. The Linux Ethernet HAL uses AF_PACKET raw sockets
# on the interface passed via --interface at runtime.
EXTRA_OECMAKE = " \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DSOES_DEMO=OFF \
"

do_install() {
    install -d ${D}${libdir}
    find ${B} -name "libsoes.a"  -exec install -m 0644 {} ${D}${libdir}/ \;
    find ${B} -name "libosal.a"  -exec install -m 0644 {} ${D}${libdir}/ \;

    install -d ${D}${includedir}/soes
    install -m 0644 ${S}/soes/esc.h       ${D}${includedir}/soes/ || true
    install -m 0644 ${S}/soes/esc_coe.h   ${D}${includedir}/soes/ || true
    install -m 0644 ${S}/soes/ecat_slv.h  ${D}${includedir}/soes/ || true
    install -m 0644 ${S}/soes/esc_cfg.h   ${D}${includedir}/soes/ || true
    # Copy any remaining public headers
    find ${S}/soes -maxdepth 1 -name "*.h" \
        -exec install -m 0644 {} ${D}${includedir}/soes/ \;
}

ALLOW_EMPTY:${PN} = "1"
FILES:${PN}-staticdev = "${libdir}/*.a"
FILES:${PN}-dev = "${includedir}"
# drive-simulator links statically — no runtime package needed
RDEPENDS:${PN} = ""
