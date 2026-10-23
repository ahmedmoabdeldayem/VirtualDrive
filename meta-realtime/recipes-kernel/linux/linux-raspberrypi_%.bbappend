FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

# Reuse the upstream raspberrypi5 BSP definition for our derived machine.
# Without this the kernel-yocto class aborts with "Could not locate BSP
# definition for raspberrypi5-rt/standard".
KMACHINE:raspberrypi5-rt = "raspberrypi5"

SRC_URI:append = " \
    file://preempt-rt.cfg \
    file://realtime.cfg \
"

# --- OPTIONAL: Full PREEMPT_RT patchset ---
# CONFIG_PREEMPT_RT=y in the config fragment requires the RT patchset.
# Without the patch, that option is silently ignored by kconfig (build still
# succeeds; other RT options like HIGH_RES_TIMERS and NO_HZ_FULL are mainline
# and will apply fine).
#
# To enable full PREEMPT_RT:
#   1. Run: bash scripts/fetch-rt-patch.sh
#      This fetches the correct patch for the kernel version in use and
#      writes a kas-rt-patch.yml overlay with the verified sha256sum.
#   2. Build with: kas build kas.yml:kas-rt-patch.yml
#
# Alternatively, set VIRTUALDRIVE_RT_PATCH_VERSION and sha256sum here:
# RT_PATCH_VERSION = "6.6.58-rt45"
# SRC_URI:append = " \
#     https://cdn.kernel.org/pub/linux/kernel/projects/rt/6.6/\
# patch-${RT_PATCH_VERSION}.patch.xz;name=rt-patch \
# "
# SRC_URI[rt-patch.sha256sum] = "<real-sha256-from-fetch-rt-patch.sh>"
