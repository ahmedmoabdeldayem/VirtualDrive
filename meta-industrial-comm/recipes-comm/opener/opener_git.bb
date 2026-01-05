SUMMARY = "OpENer — EtherNet/IP adapter stack"
DESCRIPTION = "EIPStackGroup OpENer EtherNet/IP adapter-side stack. Provides CIP class \
and assembly object management and the network I/O handler used by drive-simulator \
for the EtherNet/IP protocol adapter."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=1dece7821bf3fd70fe1309eaa37d52a2"

SRC_URI = "git://github.com/EIPStackGroup/OpENer.git;protocol=https;branch=master"
SRCREV = "${AUTOREV}"
PV = "2.0+git${SRCPV}"

S = "${WORKDIR}/git"

inherit cmake

# POSIX port provides the Linux-specific network handler and socket layer.
EXTRA_OECMAKE = " \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DOPENER_PLATFORM=POSIX \
    -DCMAKE_INSTALL_PREFIX=${prefix} \
"

# OpENer's CMake install puts headers under include/opener/
do_install:append() {
    install -d ${D}${libdir}
    find ${B} -name "libEIPStack.a" -exec install -m 0644 {} ${D}${libdir}/ \;
    find ${B} -name "libopener.a"   -exec install -m 0644 {} ${D}${libdir}/ \;

    install -d ${D}${includedir}/opener
    # Collect all public CIP headers
    for dir in ${S}/source/src/cip ${S}/source/src ${S}/source/ports/POSIX; do
        find $dir -maxdepth 1 -name "*.h" \
            -exec install -m 0644 {} ${D}${includedir}/opener/ \; 2>/dev/null || true
    done
}

ALLOW_EMPTY:${PN} = "1"
FILES:${PN}-staticdev = "${libdir}/*.a"
FILES:${PN}-dev = "${includedir}"
RDEPENDS:${PN} = ""
