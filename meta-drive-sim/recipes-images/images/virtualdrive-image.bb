SUMMARY = "VirtualDrive industrial communication image"
DESCRIPTION = "Minimal RT image with EtherCAT/PROFINET/EtherNet/IP protocol stack"

require recipes-core/images/core-image-minimal.bb

IMAGE_FEATURES += "ssh-server-openssh"

IMAGE_INSTALL:append = " \
    drive-simulator \
    \
    rt-tests \
    stress-ng \
    \
    tcpdump \
    iproute2 \
    ethtool \
    iputils \
    \
    openssh \
    openssh-sftp-server \
    \
    python3 \
    python3-core \
    python3-modules \
    \
    gdb \
    strace \
    ltrace \
    \
    htop \
    lsof \
    procps \
    \
    util-linux \
    e2fsprogs-e2fsck \
    \
    kernel-modules \
"

# Image size — RT filesystem doesn't need much space
IMAGE_ROOTFS_SIZE ?= "524288"
IMAGE_ROOTFS_EXTRA_SPACE:append = " + 65536"

# Include kernel sources for kgdb and dynamic tracing
# IMAGE_INSTALL:append = " kernel-devsrc"
