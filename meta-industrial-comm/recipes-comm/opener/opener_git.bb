SUMMARY = "OpENer EtherNet/IP adapter stack"
HOMEPAGE = "https://github.com/EIPStackGroup/OpENer"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://LICENSE;md5=2de5592e009bab5e94fc5a6b6c3abc13"

SRC_URI = "git://github.com/EIPStackGroup/OpENer.git;protocol=https;nobranch=1"
SRCREV = "${AUTOREV}"
PV = "2.3+git${SRCPV}"

S = "${WORKDIR}/git"

inherit cmake

OECMAKE_SOURCEPATH = "${S}/source"

EXTRA_OECMAKE = " \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PLATFORM=linux \
    -DBUILD_SHARED_LIBS=OFF \
"

do_install() {
    install -d ${D}${libdir}
    find ${B} -name "*.a" -exec install -m 0644 {} ${D}${libdir}/ \;

    install -d ${D}${includedir}/opener
    find ${S} -name "*.h" -not -path "*/test*" -not -path "*/.git/*" \
        | while read f; do install -m 0644 "$f" ${D}${includedir}/opener/ 2>/dev/null || true; done
}

FILES:${PN}-dev = "${includedir}/opener ${libdir}/*.a"
FILES:${PN} = ""
ALLOW_EMPTY:${PN} = "1"
