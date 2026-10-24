SUMMARY = "OpENer — EtherNet/IP adapter stack"
DESCRIPTION = "EIPStackGroup OpENer EtherNet/IP adapter-side stack. Provides CIP class \
and assembly object management and the network I/O handler used by drive-simulator \
for the EtherNet/IP protocol adapter."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://license.txt;md5=1bcf1e0089eb17f230f34b422a76b709"

SRC_URI = "git://github.com/EIPStackGroup/OpENer.git;protocol=https;branch=master"
SRCREV = "d8820ef41ce30dda06d213438ba81c81de33ffe2"
PV = "2.0+git${SRCPV}"

S = "${WORKDIR}/git"

OECMAKE_SOURCEPATH = "${S}/source"

inherit cmake

# Variable names are mixed-case in OpENer's CMake — wrong case is silently ignored.
EXTRA_OECMAKE = " \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DOpENer_PLATFORM=POSIX \
    -DOpENer_BUILDSUPPORT_DIR=${S}/source/buildsupport \
    -DCMAKE_INSTALL_PREFIX=${prefix} \
    -DCMAKE_INSTALL_FULL_BINDIR=${bindir} \
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
