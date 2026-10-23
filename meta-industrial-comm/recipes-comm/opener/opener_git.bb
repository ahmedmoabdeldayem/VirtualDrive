SUMMARY = "OpENer EtherNet/IP adapter stack (stub — library not auto-fetched)"
DESCRIPTION = "Stub recipe. drive-simulator compiles with built-in EtherNet/IP stubs \
when OpENer is absent. To enable full OpENer integration, replace this recipe \
with one that fetches https://github.com/EIPStackGroup/OpENer and \
installs libEIPStack.a + headers into sysroot."
LICENSE = "CLOSED"

do_fetch[noexec]     = "1"
do_unpack[noexec]    = "1"
do_patch[noexec]     = "1"
do_configure[noexec] = "1"
do_compile[noexec]   = "1"
do_install[noexec]   = "1"

FILES:${PN} = ""
ALLOW_EMPTY:${PN} = "1"
