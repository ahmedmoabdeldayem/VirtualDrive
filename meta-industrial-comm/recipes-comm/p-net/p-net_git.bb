SUMMARY = "p-net — PROFINET IO-Device stack"
DESCRIPTION = "rt-labs p-net PROFINET IO-Device library. The public branch ships \
source only (no build system). This recipe provides a CMakeLists.txt plus a \
Linux OSAL/PNAL implementation so p-net builds as a cross-compiled static library."
LICENSE = "GPL-3.0-only"
LIC_FILES_CHKSUM = "file://LICENSE.md;md5=330d642920859963c7f034356f2b3993"

SRC_URI = " \
    git://github.com/rtlabs-com/p-net.git;protocol=https;branch=public \
    file://CMakeLists.txt \
    file://osal.h \
    file://osal_log.h \
    file://osal_linux.c \
    file://pnal_linux.c \
    file://pnal_sys.h \
    file://pnet_export.h \
    file://pnal_config.h \
"
SRCREV = "ca4f37c5d66b5378a8cf9349b9084df0612043da"
PV = "1.0+git${SRCPV}"

S = "${WORKDIR}/git"

inherit cmake

EXTRA_OECMAKE = "-DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF"

# Inject the build system and Linux platform files into the source tree
# before cmake runs.
do_configure:prepend() {
    cp ${WORKDIR}/CMakeLists.txt  ${S}/CMakeLists.txt
    cp ${WORKDIR}/osal.h          ${S}/osal.h
    cp ${WORKDIR}/osal_log.h      ${S}/osal_log.h
    cp ${WORKDIR}/osal_linux.c    ${S}/osal_linux.c
    cp ${WORKDIR}/pnal_linux.c    ${S}/pnal_linux.c
    cp ${WORKDIR}/pnal_sys.h      ${S}/pnal_sys.h
    cp ${WORKDIR}/pnet_export.h   ${S}/include/pnet_export.h
    cp ${WORKDIR}/pnal_config.h   ${S}/include/pnal_config.h
}

do_install() {
    install -d ${D}${libdir}
    install -m 0644 ${B}/libpnet.a ${D}${libdir}/

    install -d ${D}${includedir}
    install -m 0644 ${S}/include/pnet_api.h   ${D}${includedir}/
    install -m 0644 ${S}/include/pnal.h       ${D}${includedir}/
    install -m 0644 ${S}/osal.h               ${D}${includedir}/
    install -m 0644 ${S}/osal_log.h           ${D}${includedir}/
    install -m 0644 ${S}/pnal_sys.h           ${D}${includedir}/
    install -m 0644 ${B}/pnet_options.h           ${D}${includedir}/
    install -m 0644 ${B}/pnet_version.h           ${D}${includedir}/
    install -m 0644 ${S}/include/pnet_export.h    ${D}${includedir}/
    install -m 0644 ${S}/include/pnal_config.h    ${D}${includedir}/
}

ALLOW_EMPTY:${PN} = "1"
FILES:${PN}-staticdev = "${libdir}/*.a"
FILES:${PN}-dev = "${includedir}"
RDEPENDS:${PN} = ""
