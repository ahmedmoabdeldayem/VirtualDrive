# Ensure all RT measurement tools are included in the package.
# cyclictest: measures scheduling latency of cyclic RT threads.
# hwlatdetect: measures hardware/SMI-induced latency using an NMI handler.
# Both are built by default in rt-tests but may be split into sub-packages.
RDEPENDS:${PN}:append = " numactl"

# hwlatdetect requires python3 at runtime
RDEPENDS:${PN}:append = " python3-core"
