SUMMARY = "OpENer EtherNet/IP adapter stack"
DESCRIPTION = "Open source EtherNet/IP adapter (server) stack for Linux"
HOMEPAGE = "https://github.com/EIPStackGroup/OpENer"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://LICENSE;md5=2de5592e009bab5e94fc5a6b6c3abc13"

DEPENDS = ""

# Update SRCREV with: git ls-remote https://github.com/EIPStackGroup/OpENer master
SRCREV = "3f4a5b6c7d8e9f0a1b2c3d4e5f6a7b8c9d0e1f2a"
PV = "2.3+git${SRCPV}"

SRC_URI = "git://github.com/EIPStackGroup/OpENer.git;protocol=https;branch=master"

S = "${WORKDIR}/git"

inherit cmake

# OpENer CMake root is in source/
OECMAKE_SOURCEPATH = "${S}/source"

EXTRA_OECMAKE = " \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PLATFORM=linux \
    -DBUILD_SHARED_LIBS=OFF \
    -DOPENER_INSTALL_PREFIX=${prefix} \
"

do_install() {
    install -d ${D}${libdir}
    find ${B} -name "libEIPStack*.a" -exec install -m 0644 {} ${D}${libdir}/ \; 2>/dev/null || true
    find ${B} -name "libopener*.a" -exec install -m 0644 {} ${D}${libdir}/ \; 2>/dev/null || true

    install -d ${D}${includedir}/opener
    find ${S}/source -name "*.h" -not -path "*/tests/*" \
        -exec install -m 0644 {} ${D}${includedir}/opener/ \; 2>/dev/null || true
}

FILES:${PN}-dev = "${includedir}/opener ${libdir}/libEIPStack*.a ${libdir}/libopener*.a"
FILES:${PN} = ""

ALLOW_EMPTY:${PN} = "1"
