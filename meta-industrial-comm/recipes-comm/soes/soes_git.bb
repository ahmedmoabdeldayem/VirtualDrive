SUMMARY = "Simple Open EtherCAT Slave (SOES) library"
DESCRIPTION = "EtherCAT slave stack with Linux socket-based ESC emulation"
HOMEPAGE = "https://github.com/OpenEtherCATsociety/SOES"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://LICENSE;md5=b234ee4d69f5fce4486a80fdaf4a4263"

DEPENDS = ""

# Pin to a specific commit for reproducibility.
# Update SRCREV with: git ls-remote https://github.com/OpenEtherCATsociety/SOES master
SRCREV = "a94e11d8c0d4a9a8a567ff32a37c8f4e3a214f9b"
PV = "1.0+git${SRCPV}"

SRC_URI = "git://github.com/OpenEtherCATsociety/SOES.git;protocol=https;branch=master"

S = "${WORKDIR}/git"

inherit cmake

EXTRA_OECMAKE = " \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
"

do_install() {
    install -d ${D}${libdir}
    find ${B} -name "libsoes*.a" -exec install -m 0644 {} ${D}${libdir}/ \;
    find ${B} -name "libosal*.a" -exec install -m 0644 {} ${D}${libdir}/ \;

    install -d ${D}${includedir}/soes
    install -m 0644 ${S}/soes/*.h ${D}${includedir}/soes/

    install -d ${D}${includedir}/soes/osal
    install -m 0644 ${S}/osal/*.h ${D}${includedir}/soes/osal/
    install -m 0644 ${S}/osal/linux/*.h ${D}${includedir}/soes/osal/ 2>/dev/null || true
}

FILES:${PN}-dev = "${includedir}/soes ${libdir}/libsoes*.a ${libdir}/libosal*.a"
FILES:${PN} = ""

ALLOW_EMPTY:${PN} = "1"
