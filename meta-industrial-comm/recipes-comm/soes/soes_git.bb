SUMMARY = "SOES — Simple Open EtherCAT Slave stack"
DESCRIPTION = "EtherCAT slave stack implementing CiA 301 object dictionary and process \
data exchange. Provides ecat_slv_init/ecat_slv_poll API used by drive-simulator."
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://LICENSE;md5=d86f4cd7b881c753fe81c48d2fb8baf4"

SRC_URI = " \
    git://github.com/OpenEtherCATsociety/SOES.git;protocol=https;branch=master \
    file://cc.h \
    file://ecat_options.h \
"
SRCREV = "6ef7b9415e936c061ad30626ca00004bdde2ac62"
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

    # cc.h is included with angle brackets by soes/esc.h — must be in system include path
    install -d ${D}${includedir}
    install -m 0644 ${WORKDIR}/cc.h ${D}${includedir}/cc.h

    # Install headers in both soes/ (for #include "soes/esc.h" callers) and
    # directly in includedir (for <esc_coe.h> angle-bracket includes within esc.h)
    install -d ${D}${includedir}/soes
    find ${S}/soes -maxdepth 1 -name "*.h" \
        -exec install -m 0644 {} ${D}${includedir}/soes/ \;
    find ${S}/soes -maxdepth 1 -name "*.h" \
        -exec install -m 0644 {} ${D}${includedir}/ \;
    # ecat_options.h is a user config included by options.h with quotes
    install -m 0644 ${WORKDIR}/ecat_options.h ${D}${includedir}/soes/ecat_options.h
    install -m 0644 ${WORKDIR}/ecat_options.h ${D}${includedir}/ecat_options.h
}

ALLOW_EMPTY:${PN} = "1"
FILES:${PN}-staticdev = "${libdir}/*.a"
FILES:${PN}-dev = "${includedir}"
# drive-simulator links statically — no runtime package needed
RDEPENDS:${PN} = ""
