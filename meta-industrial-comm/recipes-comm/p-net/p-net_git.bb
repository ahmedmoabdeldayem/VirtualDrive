SUMMARY = "p-net PROFINET IO-Device stack (stub — library not auto-fetched)"
DESCRIPTION = "Stub recipe. drive-simulator compiles with built-in PROFINET stubs \
when p-net is absent. To enable full p-net integration, replace this recipe \
with one that fetches https://github.com/rtlabs-com/p-net and \
installs libpnet.a + headers into sysroot."
LICENSE = "CLOSED"

do_fetch[noexec]     = "1"
do_unpack[noexec]    = "1"
do_patch[noexec]     = "1"
do_configure[noexec] = "1"
do_compile[noexec]   = "1"
do_install[noexec]   = "1"

FILES:${PN} = ""
ALLOW_EMPTY:${PN} = "1"
