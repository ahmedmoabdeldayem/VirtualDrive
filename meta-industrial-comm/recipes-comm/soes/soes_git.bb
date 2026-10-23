SUMMARY = "Simple Open EtherCAT Slave (SOES) library"
HOMEPAGE = "https://github.com/OpenEtherCATsociety/SOES"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://LICENSE;md5=b234ee4d69f5fce4486a80fdaf4a4263"

SRC_URI = "git://github.com/OpenEtherCATsociety/SOES.git;protocol=https;nobranch=1"
SRCREV = "${AUTOREV}"
PV = "1.0+git${SRCPV}"

S = "${WORKDIR}/git"

inherit cmake

EXTRA_OECMAKE = "-DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF"

do_install() {
    # Install any static libraries found anywhere in the build tree
    install -d ${D}${libdir}
    find ${B} -name "*.a" -exec install -m 0644 {} ${D}${libdir}/ \;

    # Install all headers, preserving useful subdirectory structure
    install -d ${D}${includedir}/soes
    find ${S} -name "*.h" -not -path "*/test/*" -not -path "*/.git/*" | while read f; do
        install -m 0644 "$f" ${D}${includedir}/soes/ 2>/dev/null || true
    done
}

FILES:${PN}-dev = "${includedir}/soes ${libdir}/*.a"
FILES:${PN} = ""
ALLOW_EMPTY:${PN} = "1"
BBCLASSEXTEND = "native nativesdk"
