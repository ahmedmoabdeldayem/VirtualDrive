SUMMARY = "p-net PROFINET IO-Device library"
DESCRIPTION = "Open source PROFINET IO-Device stack targeting embedded Linux"
HOMEPAGE = "https://github.com/rtlabs-com/p-net"
LICENSE = "BSD-3-Clause & GPL-2.0-only"
LIC_FILES_CHKSUM = "file://LICENSE.md;md5=e7e5ceeb003c5e39e4bf8ceda8a4e5fc"

DEPENDS = ""

# Update SRCREV with: git ls-remote https://github.com/rtlabs-com/p-net main
SRCREV = "7e1f8b3a2c4d6e0f9a8b7c3d2e1f4a5b6c7d8e9f"
PV = "1.0+git${SRCPV}"

SRC_URI = "git://github.com/rtlabs-com/p-net.git;protocol=https;branch=main"

S = "${WORKDIR}/git"

inherit cmake

EXTRA_OECMAKE = " \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTING=OFF \
    -DBUILD_DOCUMENTATION=OFF \
    -DUSE_SCHED_FIFO=ON \
"

do_install() {
    install -d ${D}${libdir}
    find ${B} -name "libpnet*.a" -exec install -m 0644 {} ${D}${libdir}/ \;

    install -d ${D}${includedir}/pnet
    install -m 0644 ${S}/include/*.h ${D}${includedir}/pnet/ 2>/dev/null || true
    install -m 0644 ${S}/src/pnet_api.h ${D}${includedir}/ 2>/dev/null || true

    # OSAL headers needed at compile time
    install -d ${D}${includedir}/osal
    find ${S}/osal -name "*.h" -exec install -m 0644 {} ${D}${includedir}/osal/ \; 2>/dev/null || true
}

FILES:${PN}-dev = "${includedir}/pnet ${includedir}/osal ${includedir}/pnet_api.h ${libdir}/libpnet*.a"
FILES:${PN} = ""

ALLOW_EMPTY:${PN} = "1"
