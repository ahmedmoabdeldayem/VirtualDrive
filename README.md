# VirtualDrive

A software-simulated industrial drive controller on a Raspberry Pi 5.
Implements EtherCAT slave (SOES), PROFINET IO-Device (p-net), and
EtherNet/IP adapter (OpENer) over a single Gigabit Ethernet port.
No extra hardware beyond the RPi5 and a PC connected via Ethernet.

## Learning goals

| Topic | Where it appears |
|---|---|
| EtherCAT slave stack (SOES) | `EtherCATAdapter.cpp`, `meta-industrial-comm/soes_git.bb` |
| PROFINET IO-Device (p-net) | `PROFINETAdapter.cpp`, `virtualdrive.gsdml` |
| EtherNet/IP adapter (OpENer) | `EtherNetIPAdapter.cpp` |
| PREEMPT_RT kernel | `meta-realtime/` layer, `preempt-rt.cfg` |
| CiA 402 drive state machine | `DriveStateMachine.cpp` |
| RT cyclic thread (POSIX) | `CyclicThread.cpp` |
| Layered HAL design | `SerialLoopback.cpp` (SPI/I2C/UART loopback) |
| Motor physics simulation | `MotorModel.cpp` |
| POSIX HTTP monitoring | `DriveMonitor.cpp` |
| Yocto Scarthgap meta-layer | `meta-realtime/`, `meta-industrial-comm/`, `meta-drive-sim/` |
| GoogleTest unit tests | `test/` directory |

---

## 1. Build with KAS on UTM (recommended for macOS)

KAS is a Yocto setup tool that replaces manual `bblayers.conf`/`local.conf`
management. UTM is a free VM app for macOS. Together they give you a clean
Ubuntu build environment on your Mac.

### 1.1 Create the UTM VM

```
App:          https://mac.getutm.app  (free)
Ubuntu ISO:   ARM64 for Apple Silicon (M1/M2/M3/M4):
              https://cdimage.ubuntu.com/releases/22.04/release/ubuntu-22.04.5-live-server-arm64.iso
              x86_64 for Intel Mac:
              https://releases.ubuntu.com/22.04/ubuntu-22.04.5-live-server-amd64.iso

VM settings:
  OS:     Linux
  Arch:   ARM64 (Apple Silicon) or x86_64 (Intel)
  RAM:    8 GB minimum, 16 GB recommended
  CPUs:   all cores
  Disk:   200 GB  (Yocto builds are large)
  ISO:    select the Ubuntu ISO above

Sharing:
  VM Settings > Sharing > Enable Directory Sharing
  Add your VirtualDrive project folder, Tag = "mac-share"
```

Install Ubuntu with OpenSSH enabled. After first boot, find the VM IP:

```bash
# Inside the UTM terminal:
ip addr show | grep 'inet '
```

### 1.2 Set up the build environment (run once in the VM)

```bash
# From your Mac, copy the setup script into the VM:
scp scripts/utm-vm-setup.sh ubuntu@<VM-IP>:~/

# Run it (installs Yocto deps + KAS, shows full instructions):
ssh ubuntu@<VM-IP> 'bash utm-vm-setup.sh'
```

Or if you share the project folder via UTM VirtFS, mount it inside the VM:

```bash
# Inside the VM:
sudo apt-get install -y mount
mkdir -p ~/mac-share
sudo mount -t 9p -o trans=virtio,version=9p2000.L,rw mac-share ~/mac-share
# Your project is now at ~/mac-share/
```

### 1.3 Build

```bash
# Inside the VM, in the VirtualDrive project root:
cd ~/yocto/VirtualDrive     # or ~/mac-share if using VirtFS

# (Optional) fetch verified PREEMPT_RT patch:
bash scripts/fetch-rt-patch.sh

# Build the full image (takes 2–4 h on first build; much faster with sstate):
kas build kas.yml

# Build the cross-compilation SDK (to rebuild the C++ app quickly on Mac):
kas build kas.yml:kas-sdk.yml

# Open a BitBake devshell for a specific recipe:
kas shell kas.yml
bitbake -c devshell drive-simulator
```

### 1.4 Copy the image back to macOS

```bash
# From your Mac:
scp ubuntu@<VM-IP>:~/yocto/VirtualDrive/build/tmp/deploy/images/\
raspberrypi5-rt/virtualdrive-image-raspberrypi5-rt.wic.bz2 .
```

Flash with `dd` or Raspberry Pi Imager (see §4 below).

> **Tip — sstate cache:** After the first full build the sstate cache is
> ~15–20 GB on the VM disk. Subsequent `kas build` runs take 5–10 minutes
> (only changed recipes rebuild). Keep the VM around.

---

## 2. Prerequisites — host packages for Yocto build (manual, no KAS)

Tested on Ubuntu 22.04 LTS (x86_64).

```bash
sudo apt-get update
sudo apt-get install -y \
    gawk wget git diffstat unzip texinfo gcc build-essential \
    chrpath socat cpio python3 python3-pip python3-pexpect \
    xz-utils debianutils iputils-ping python3-git python3-jinja2 \
    libegl1-mesa libsdl1.2-dev python3-subunit mesa-common-dev zstd \
    liblz4-tool file locales libacl1
sudo locale-gen en_US.UTF-8
```

Disk space: ~100 GB free recommended for a full build with sstate cache.

---

## 3. Build instructions (manual, without KAS)

### 3.1 Clone external layers alongside this repo

```bash
# Working directory example: ~/yocto/
mkdir -p ~/yocto && cd ~/yocto

git clone https://github.com/VirtualDrive/VirtualDrive.git
git clone https://git.yoctoproject.org/poky.git                          -b scarthgap
git clone https://github.com/agherzan/meta-raspberrypi.git               -b scarthgap
git clone https://github.com/openembedded/meta-openembedded.git          -b scarthgap
```

Directory layout after cloning:

```
~/yocto/
  VirtualDrive/          <- this repo
  poky/
  meta-raspberrypi/
  meta-openembedded/
```

### 3.2 Update SRCREV pins

Before building, verify SOES / p-net / OpENer commit hashes are current:

```bash
git ls-remote https://github.com/OpenEtherCATsociety/SOES.git master
git ls-remote https://github.com/rtlabs-com/p-net.git main
git ls-remote https://github.com/EIPStackGroup/OpENer.git master
```

Edit the SRCREV lines in:
- `meta-industrial-comm/recipes-comm/soes/soes_git.bb`
- `meta-industrial-comm/recipes-comm/p-net/p-net_git.bb`
- `meta-industrial-comm/recipes-comm/opener/opener_git.bb`

Also update the RT patch URL in
`meta-realtime/recipes-kernel/linux/linux-raspberrypi_%.bbappend`
to match the actual kernel version used by `linux-raspberrypi` in Scarthgap.

### 3.3 Initialize the build environment

```bash
cd ~/yocto
source poky/oe-init-build-env VirtualDrive/build
```

The `build/conf/` directory already contains `bblayers.conf` and `local.conf`
from this repo — they are ready to use.

### 3.4 Build the image

```bash
bitbake virtualdrive-image
```

The compressed SD card image is at:
```
build/tmp/deploy/images/raspberrypi5-rt/virtualdrive-image-raspberrypi5-rt.wic.bz2
```

---

## 4. Flashing instructions

### Using rpi-imager (GUI)

1. Open Raspberry Pi Imager
2. Choose OS → Use custom → select the `.wic.bz2` file
3. Choose storage → your SD card
4. Write

### Using dd (CLI)

```bash
bzcat virtualdrive-image-raspberrypi5-rt.wic.bz2 | \
    sudo dd of=/dev/sdX bs=4M conv=fsync status=progress
```

Replace `/dev/sdX` with your SD card device.

---

## 5. First boot and network setup

### 5.1 Boot

Insert the SD card, connect the RPi5 Ethernet port to your PC via a direct
cable or a switch. Power on. The drive-simulator service starts automatically
via systemd on interface `eth0`.

### 5.2 Find the RPi5 IP

From the PC:

```bash
# If RPi5 and PC are on the same subnet with DHCP:
arp-scan --interface=<your-pc-iface> --localnet | grep -i raspberry

# Or check your router's DHCP table, or use nmap:
nmap -sn 192.168.1.0/24
```

### 5.3 Configure a static IP (optional, recommended for EtherCAT)

Edit `/etc/systemd/network/10-eth0.network` on the RPi5:

```ini
[Match]
Name=eth0

[Network]
Address=192.168.1.100/24
Gateway=192.168.1.1
DNS=8.8.8.8
```

Then: `sudo systemctl restart systemd-networkd`

---

## 6. Running each protocol mode

The systemd service defaults to EtherCAT. To change protocol, edit
`/lib/systemd/system/drive-simulator.service` on the RPi5:

```bash
# For EtherCAT (default):
ExecStart=/usr/bin/drive-simulator --protocol ethercat --interface eth0

# For PROFINET:
ExecStart=/usr/bin/drive-simulator --protocol profinet --interface eth0

# For EtherNet/IP:
ExecStart=/usr/bin/drive-simulator --protocol ethernetip --interface eth0
```

Then:

```bash
sudo systemctl daemon-reload
sudo systemctl restart drive-simulator
sudo journalctl -fu drive-simulator
```

Or run it directly as root (for SCHED_FIFO capability):

```bash
sudo /usr/bin/drive-simulator --protocol ethercat --interface eth0
```

---

## 7. PC-side tool setup for each protocol

### 7.1 EtherCAT master (SOEM)

```bash
# On Ubuntu PC, connected to RPi5 via <iface>, e.g. enp3s0
sudo bash scripts/setup_ethercat_master.sh enp3s0

# Run the test master (RPi5 must be running with --protocol ethercat):
sudo ~/soem-test/vd_master enp3s0
```

Expected output:
```
[Master] 1 slave(s) found
[Master] Slave in OPERATIONAL — starting 1ms cycle
[   0] SW=0x0037  vel=1500 RPM  pos=1500
[   1] SW=0x0037  vel=1500 RPM  pos=3000
```

### 7.2 PROFINET controller

See `scripts/setup_profinet_controller.sh` for full instructions.
Quick summary:

- **Recommended**: CODESYS Development System V3 (free download, Windows).
  Import `virtualdrive.gsdml` and set the station name to `virtualdrive`.
- **Alternative**: Wireshark + manual DCP frames for bus observation without
  a full PLC.

```bash
sudo bash scripts/setup_profinet_controller.sh
```

### 7.3 EtherNet/IP scanner (Python/cpppo)

```bash
pip install cpppo
python3 scripts/setup_ethernetip_scanner.py 192.168.1.100
```

Expected output:
```
[Scanner] Identity response: ...
[    0]  SW=0x0037  speed=  1500.0 RPM  pos=     1500  rtt=3.2 ms
[    1]  SW=0x0037  speed=  1500.0 RPM  pos=     3000  rtt=3.1 ms
```

---

## 8. Running unit tests

Unit tests run on the host (no cross-compilation needed for testing the
portable drive logic):

```bash
# Build native (on Ubuntu PC):
cd meta-drive-sim/recipes-drive/drive-simulator/files
cmake -B build-host -DBUILD_TESTING=ON \
      -DCMAKE_BUILD_TYPE=Debug \
      -DSOES_LIB="" -DPNET_LIB="" -DOPENER_LIB=""  # stubs — not needed for unit tests
```

If the native build fails due to missing protocol library stubs, build only
the test targets that don't depend on them:

```bash
cmake -B build-host -DBUILD_TESTING=ON
cd build-host
cmake --build . --target DriveStateMachineTest MotorModelTest CyclicThreadTest
ctest --output-on-failure
```

Expected:

```
[==========] Running 18 tests from 3 test suites.
[----------] 11 tests from DSMTest
[ RUN      ] DSMTest.StartsInSwitchOnDisabled          ...  PASSED
...
[  PASSED  ] 18 tests.
```

---

## 9. Measuring RT latency

Run this **on the RPi5**, not the PC:

```bash
sudo bash /usr/share/drive-simulator/../scripts/measure_latency.sh 60
```

Or copy the script:

```bash
scp scripts/measure_latency.sh root@<rpi5-ip>:/tmp/
ssh root@<rpi5-ip> "sudo bash /tmp/measure_latency.sh 60"
```

Expected results on a correctly configured PREEMPT_RT system:

```
  Total samples: 240,000
  Min:          2 µs
  P50:          5 µs
  P99:         18 µs
  P99.9:       42 µs
  P99.99:      68 µs
  Max:         95 µs
  Latency < 100 µs (RT target): PASS
```

If P99.9 > 200 µs, check:
1. `CONFIG_PREEMPT_RT=y` is in the running kernel: `zcat /proc/config.gz | grep PREEMPT_RT`
2. `isolcpus=3` is in the kernel cmdline: `cat /proc/cmdline`
3. `force_turbo=1` is in `config.txt` (prevents frequency scaling)
4. No USB 3.0 devices attached (USB 3.0 DMA can cause SMI latency)

---

## 10. HTTP monitoring

The drive-simulator exposes a simple HTTP server on port 8080.

```bash
# Drive status as JSON:
curl http://<rpi5-ip>:8080/status

# Health check (suitable for a load balancer probe):
curl http://<rpi5-ip>:8080/health
```

Example `/status` response:

```json
{
  "state": "OPERATION_ENABLED",
  "actualSpeed": 1498.7,
  "targetSpeed": 1500.0,
  "position": 249783,
  "torque": 0.65,
  "temperature": 28.3,
  "statusWord": 55,
  "controlWord": 15,
  "cyclicMissCount": 0,
  "worstJitterUs": 43,
  "protocolName": "EtherCAT"
}
```

To watch continuously:

```bash
watch -n1 'curl -s http://<rpi5-ip>:8080/status | python3 -m json.tool'
```

---

## 11. Troubleshooting

### Common Yocto errors

**`ERROR: No recipes available for 'linux-raspberrypi'`**
→ Verify `meta-raspberrypi` is checked out to the `scarthgap` branch and is
listed in `bblayers.conf`.

**`ERROR: Fetcher failure for URL 'git://...'`**
→ Network issue or wrong SRCREV. Update the SRCREV in the affected `.bb` file
and re-run `bitbake`.

**`ERROR: QA Issue: ... installed but not shipped`**
→ The library was built but not listed in `FILES`. Update the `FILES:${PN}`
or `FILES:${PN}-dev` variable in the affected recipe.

**`Fetch failed: … sha256 mismatch`**
→ The RT patch URL for linux 6.6 has changed. Update `RT_PATCH_VERSION`
and the `sha256sum` in `linux-raspberrypi_%.bbappend`.

### SOES / EtherCAT

**`ec_init failed` on the PC-side master**
→ The PC NIC must be on the same L2 segment as the RPi5 (direct cable or
unmanaged switch). EtherCAT uses raw Ethernet frames (ethertype 0x88A4) which
are blocked by managed switches with port isolation.

**SOES stays in PREOP, never reaches OPERATIONAL**
→ The object dictionary (SDOobjects[]) must exactly match the PDO mapping
objects. Verify SDO 0x1C12 and 0x1C13 (PDO assignment) are correct. SOES
logs ESM transitions to stdout.

**`cb_set_outputs` not called**
→ SOES calls the PDO callbacks only in OP state. The master must explicitly
set EC_STATE_OPERATIONAL. Check `ecat_slv_poll` is being called in a loop.

### p-net / PROFINET

**Device not visible in CODESYS topology scan**
→ Verify the RPi5 IP is reachable: `ping 192.168.1.100`. PROFINET DCP
discovery uses broadcast (MAC FF:FF:FF:FF:FF:FF) — ensure your PC NIC
is on the same broadcast domain (same VLAN, unmanaged switch, or direct cable).

**`pnet_init` returns NULL**
→ The `eth_interface` field in `pnet_cfg_t` must match the actual interface
name on the RPi5 (e.g., `eth0`). Check with `ip link show`.

**GSDML import fails in CODESYS**
→ The XML namespace must match the GSDML schema version. The GSDML in this
repo targets GSDML 2.35. If your engineering tool requires an older version,
update the `xsi:schemaLocation` attribute.

### EtherNet/IP

**`NetworkHandlerInitialize` fails**
→ The IP address passed to `CipStackInit()` must match the RPi5 interface
address. OpENer binds UDP port 2222 and TCP port 44818 — confirm they are
not in use: `ss -tlnup | grep -E '2222|44818'`.

**`cpppo` reports `Connection refused`**
→ The drive-simulator must be started with `--protocol ethernetip` before
running the Python scanner. TCP 44818 must be reachable (no firewall).

### Network interface for EtherCAT raw socket

EtherCAT (SOEM/SOES) uses a raw `AF_PACKET` socket. The process needs
`CAP_NET_RAW`. The systemd service grants this via `AmbientCapabilities`.
If running manually, use `sudo` or set the capability:

```bash
sudo setcap cap_net_raw+ep /usr/bin/drive-simulator
```

---

## Project directory layout

```
VirtualDrive/
├── build/conf/                   # Yocto build configuration
│   ├── bblayers.conf
│   └── local.conf
├── meta-realtime/                # PREEMPT_RT kernel layer
├── meta-industrial-comm/         # SOES, p-net, OpENer recipes
├── meta-drive-sim/               # Application layer
│   ├── conf/distro/virtualdrive.conf
│   ├── conf/machine/raspberrypi5-rt.conf
│   ├── recipes-drive/drive-simulator/files/
│   │   ├── CMakeLists.txt
│   │   ├── src/
│   │   │   ├── main.cpp
│   │   │   ├── DriveSimulator.{hpp,cpp}
│   │   │   ├── drive/           DriveStateMachine, MotorModel
│   │   │   ├── comm/            IProtocolAdapter, EtherCAT/PROFINET/EtherNetIP adapters
│   │   │   ├── rt/              CyclicThread
│   │   │   ├── hal/             SerialLoopback
│   │   │   └── monitoring/      DriveMonitor (HTTP)
│   │   ├── test/                GoogleTest unit tests
│   │   └── virtualdrive.gsdml   PROFINET device description
│   └── recipes-images/images/virtualdrive-image.bb
└── scripts/
    ├── setup_ethercat_master.sh
    ├── setup_profinet_controller.sh
    ├── setup_ethernetip_scanner.py
    └── measure_latency.sh
```
