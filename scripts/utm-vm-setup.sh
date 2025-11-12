#!/usr/bin/env bash
# UTM VM setup script for building VirtualDrive with KAS on macOS.
# Run this INSIDE the Ubuntu VM after first boot.
#
# Tested with: Ubuntu 22.04.x Server ARM64 (for Apple Silicon M1/M2/M3/M4)
#              Ubuntu 22.04.x Server x86_64 (for Intel Mac or Rosetta emulation)
#
# Quickstart:
#   1. Create the UTM VM (see instructions printed at the bottom of this script)
#   2. Boot the VM and SSH into it, or open its terminal
#   3. Run: bash utm-vm-setup.sh
#   4. cd VirtualDrive && kas build kas.yml
set -euo pipefail

REPO_URL="https://github.com/VirtualDrive/VirtualDrive.git"   # set to your fork/local path
WORKDIR="${HOME}/yocto"
BUILD_THREADS="$(nproc)"

echo "========================================================"
echo " VirtualDrive / UTM VM Build Environment Setup"
echo " Host:    $(uname -m)"
echo " Threads: ${BUILD_THREADS}"
echo "========================================================"
echo ""

# ---------------------------------------------------------------------------
# 1. Yocto host dependencies (Ubuntu 22.04)
# ---------------------------------------------------------------------------
echo "==> Installing Yocto host dependencies ..."
sudo apt-get update -qq
sudo apt-get install -y --no-install-recommends \
    gawk wget git diffstat unzip texinfo gcc build-essential \
    chrpath socat cpio python3 python3-pip python3-pexpect \
    xz-utils debianutils iputils-ping python3-git python3-jinja2 \
    libegl1-mesa libsdl1.2-dev python3-subunit mesa-common-dev \
    zstd liblz4-tool file locales libacl1 \
    lz4 zstd curl ca-certificates \
    # KAS dependencies
    python3-yaml python3-distutils

# Generate en_US.UTF-8 locale (required by Yocto)
sudo locale-gen en_US.UTF-8
echo 'export LANG=en_US.UTF-8' >> "${HOME}/.bashrc"
export LANG=en_US.UTF-8

# ---------------------------------------------------------------------------
# 2. Install KAS
# ---------------------------------------------------------------------------
echo "==> Installing KAS ..."
pip3 install --user kas
# Ensure ~/.local/bin is in PATH for the current session
export PATH="${HOME}/.local/bin:${PATH}"
echo 'export PATH="${HOME}/.local/bin:${PATH}"' >> "${HOME}/.bashrc"

KAS_VERSION=$(kas --version 2>&1 || echo "unknown")
echo "    KAS: ${KAS_VERSION}"

# ---------------------------------------------------------------------------
# 3. Workspace setup
# ---------------------------------------------------------------------------
echo "==> Creating workspace at ${WORKDIR} ..."
mkdir -p "${WORKDIR}"
cd "${WORKDIR}"

# ---------------------------------------------------------------------------
# 4. Get the VirtualDrive repo
# ---------------------------------------------------------------------------
if [[ -d "${WORKDIR}/VirtualDrive/.git" ]]; then
    echo "==> VirtualDrive repo already present, skipping clone"
else
    # Option A: clone from GitHub (replace with your fork URL)
    echo "==> Cloning VirtualDrive ..."
    git clone "${REPO_URL}" "${WORKDIR}/VirtualDrive" || {
        echo ""
        echo "NOTE: Clone failed (no remote set up yet)."
        echo "      Mount your macOS project folder via UTM VirtFS instead:"
        echo "      See 'Sharing macOS folder into UTM VM' below."
    }
fi

# ---------------------------------------------------------------------------
# 5. (Optional) Mount macOS shared folder
# ---------------------------------------------------------------------------
# If you mounted the macOS VirtualDrive folder via UTM VirtFS, mount it:
# (UTM > VM Settings > Sharing > Enable VirtFS, share folder, tag = 'mac-share')
#
# sudo apt-get install -y 9mount  # or use mount directly
# mkdir -p ~/mac-share
# sudo mount -t 9p -o trans=virtio,version=9p2000.L,rw mac-share ~/mac-share
# cd ~/mac-share  # your macOS VirtualDrive folder is here
# kas build kas.yml  # build directly from the shared folder
#
# NOTE: KAS downloads (dl/) and sstate-cache/ should stay on the VM's local
# disk for performance. The kas.yml sets:
#   DL_DIR    = ${TOPDIR}/../dl          <- inside VM's local build tree
#   SSTATE_DIR = ${TOPDIR}/../sstate     <- inside VM's local build tree
# So even if the source is on VirtFS, heavy I/O goes to the local disk.

# ---------------------------------------------------------------------------
# 6. Reserve disk space for the build
# ---------------------------------------------------------------------------
echo "==> Checking available disk space ..."
AVAILABLE_GB=$(df -BG "${WORKDIR}" | tail -1 | awk '{print $4}' | tr -d G)
echo "    Available: ${AVAILABLE_GB} GB"
if (( AVAILABLE_GB < 80 )); then
    echo "    WARNING: Yocto needs ~80–120 GB free. Expand the VM disk if needed."
    echo "    UTM: VM Settings > Drives > resize the disk"
    echo "    Then inside VM: sudo growpart /dev/vda 1 && sudo resize2fs /dev/vda1"
fi

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------
echo ""
echo "========================================================"
echo " Setup complete!"
echo "========================================================"
echo ""
echo "Build commands:"
echo ""
echo "  cd ${WORKDIR}/VirtualDrive"
echo ""
echo "  # Step 1 (optional but recommended): fetch verified PREEMPT_RT patch"
echo "  bash scripts/fetch-rt-patch.sh"
echo ""
echo "  # Step 2: full image build"
echo "  kas build kas.yml"
echo ""
echo "  # Build SDK for cross-development on the Mac:"
echo "  kas build kas.yml:kas-sdk.yml"
echo ""
echo "  # Open a bitbake shell (for debugging individual recipes):"
echo "  kas shell kas.yml"
echo "  bitbake -c devshell drive-simulator"
echo ""
echo "Output image location after build:"
echo "  ${WORKDIR}/VirtualDrive/build/tmp/deploy/images/raspberrypi5-rt/"
echo "  File: virtualdrive-image-raspberrypi5-rt.wic.bz2"
echo ""
echo "Copy image back to macOS (from macOS terminal):"
echo "  scp ubuntu@<VM-IP>:${WORKDIR}/VirtualDrive/build/tmp/deploy/images/"
echo "       raspberrypi5-rt/virtualdrive-image-raspberrypi5-rt.wic.bz2 ."
echo ""
echo "---"
echo ""
echo "UTM VM creation steps (do these on your Mac FIRST):"
echo ""
echo "  1. Download UTM: https://mac.getutm.app  (free, open-source)"
echo ""
echo "  2. Download Ubuntu 22.04 Server ISO:"
echo "     Apple Silicon (M1/M2/M3/M4) ARM64:"
echo "       https://cdimage.ubuntu.com/releases/22.04/release/ubuntu-22.04.5-live-server-arm64.iso"
echo "     Intel Mac x86_64:"
echo "       https://releases.ubuntu.com/22.04/ubuntu-22.04.5-live-server-amd64.iso"
echo ""
echo "  3. In UTM: + > Virtualize (Apple Silicon) or Emulate (Intel):"
echo "     OS: Linux"
echo "     Architecture: ARM64 (Apple Silicon) or x86_64 (Intel)"
echo "     Memory: 8192 MB (8 GB minimum, 16 GB recommended)"
echo "     CPU: all cores"
echo "     Disk: 200 GB (Yocto builds are large)"
echo "     Boot ISO: select the Ubuntu ISO"
echo ""
echo "  4. Enable file sharing:"
echo "     VM Settings > Sharing > Enable Directory Sharing"
echo "     Add your VirtualDrive project folder, Tag: 'mac-share'"
echo ""
echo "  5. Install Ubuntu normally (minimal server install)"
echo "     Enable OpenSSH server during install for remote access"
echo ""
echo "  6. After first boot, copy this script into the VM and run it:"
echo "     scp scripts/utm-vm-setup.sh ubuntu@<VM-IP>:~/"
echo "     ssh ubuntu@<VM-IP> 'bash utm-vm-setup.sh'"
echo ""
echo "  7. To find the VM IP:"
echo "     UTM terminal: ip addr show | grep 'inet '"
