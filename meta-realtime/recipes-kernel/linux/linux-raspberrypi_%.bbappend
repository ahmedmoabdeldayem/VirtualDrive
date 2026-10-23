FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

# Reuse the upstream raspberrypi5 BSP definition for our derived machine.
KMACHINE:raspberrypi5-rt = "raspberrypi5"

# meta-raspberrypi sets KBUILD_DEFCONFIG only for MACHINE=raspberrypi5.
# Since our MACHINE=raspberrypi5-rt that override is never applied, so
# kernel-yocto reports "no defconfig was provided".  Mirror it here.
KBUILD_DEFCONFIG:raspberrypi5-rt = "bcm2712_defconfig"

SRC_URI:append = " \
    file://preempt-rt.cfg \
    file://realtime.cfg \
"

# The kernel-meta RT patch set does not apply cleanly to RPi5 6.6.x.
# Remove it so do_patch does not fail. RT-adjacent options (HIGH_RES_TIMERS,
# NO_HZ_FULL, HZ_1000) are mainline and still applied via the cfg fragments.
# CONFIG_PREEMPT_RT requires the patchset and will be silently ignored.
KERNEL_FEATURES:remove = "features/rt/rt.scc"
KERNEL_FEATURES:remove = "features/rt/preempt-rt.scc"

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
