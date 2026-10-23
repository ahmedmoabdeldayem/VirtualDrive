SUMMARY = "p-net PROFINET IO-Device library"
HOMEPAGE = "https://github.com/rtlabs-com/p-net"
LICENSE = "BSD-3-Clause & GPL-2.0-only"
LIC_FILES_CHKSUM = "file://LICENSE.md;md5=e7e5ceeb003c5e39e4bf8ceda8a4e5fc"

SRC_URI = "git://github.com/rtlabs-com/p-net.git;protocol=https;branch=master"
SRCREV = "${AUTOREV}"
PV = "1.0+git${SRCPV}"

S = "${WORKDIR}/git"

inherit cmake

EXTRA_OECMAKE = " \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTING=OFF \
    -DBUILD_DOCUMENTATION=OFF \
"

do_install() {
    install -d ${D}${libdir}
    find ${B} -name "*.a" -exec install -m 0644 {} ${D}${libdir}/ \;

    install -d ${D}${includedir}
    # p-net public API header
    find ${S} -name "pnet_api.h" -exec install -m 0644 {} ${D}${includedir}/ \; 2>/dev/null || true
    find ${S} -maxdepth 3 -name "*.h" -not -path "*/test/*" -not -path "*/.git/*" \
        | while read f; do install -m 0644 "$f" ${D}${includedir}/ 2>/dev/null || true; done
}

FILES:${PN}-dev = "${includedir} ${libdir}/*.a"
FILES:${PN} = ""
ALLOW_EMPTY:${PN} = "1"
