SUMMARY = "p-net — PROFINET IO-Device stack"
DESCRIPTION = "rt-labs p-net PROFINET IO-Device library. Implements a full PROFINET \
IO-Device with cyclic I/O, alarms, and parameter access. Used by drive-simulator \
for the PROFINET protocol adapter."
LICENSE = "GPL-3.0-only"
LIC_FILES_CHKSUM = "file://LICENSE.md;md5=330d642920859963c7f034356f2b3993"

SRC_URI = "git://github.com/rtlabs-com/p-net.git;protocol=https;branch=public"
SRCREV = "ca4f37c5d66b5378a8cf9349b9084df0612043da"
PV = "1.0+git${SRCPV}"

S = "${WORKDIR}/git"

inherit cmake

EXTRA_OECMAKE = " \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTING=OFF \
    -DBUILD_SAMPLES=OFF \
    -DUSE_SCHED_FIFO=ON \
"

do_install() {
    install -d ${D}${libdir}
    find ${B} -name "libpnet.a"  -exec install -m 0644 {} ${D}${libdir}/ \;
    # p-net may also produce a platform abstraction library
    find ${B} -name "libpnal.a"  -exec install -m 0644 {} ${D}${libdir}/ \;
    find ${B} -name "libosal.a"  -exec install -m 0644 {} ${D}${libdir}/ \;

    install -d ${D}${includedir}
    install -m 0644 ${S}/include/pnet_api.h ${D}${includedir}/ || true
    find ${S}/include -maxdepth 1 -name "*.h" \
        -exec install -m 0644 {} ${D}${includedir}/ \;
    # Platform headers (osal.h lives here in some versions)
    find ${S}/src/ports/linux -maxdepth 1 -name "*.h" \
        -exec install -m 0644 {} ${D}${includedir}/ \; 2>/dev/null || true
}

ALLOW_EMPTY:${PN} = "1"
FILES:${PN}-staticdev = "${libdir}/*.a"
FILES:${PN}-dev = "${includedir}"
RDEPENDS:${PN} = ""
