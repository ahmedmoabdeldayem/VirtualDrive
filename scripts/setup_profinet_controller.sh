#!/usr/bin/env bash
# PC-side PROFINET IO-Controller setup
# Option A: CODESYS Runtime (commercial, requires manual download)
# Option B: p-net sample_app as a simple open-source IO-Controller test tool
set -euo pipefail

echo "========================================================"
echo " VirtualDrive PROFINET Controller Setup"
echo "========================================================"
echo ""

# ---------------------------------------------------------------------------
# Option A: CODESYS Runtime (requires account at codesys.com — free download)
# ---------------------------------------------------------------------------
echo "--- Option A: CODESYS Runtime (recommended for full PLC simulation) ---"
echo ""
echo "MANUAL STEPS required (cannot be automated — download requires login):"
echo ""
echo "  1. Create a free account at https://www.codesys.com"
echo "  2. Download: CODESYS Development System V3 (Windows) OR"
echo "               CODESYS Runtime for Raspberry Pi / Linux (arm64 deb)"
echo "  3. On Windows: File > New Project > Standard Project"
echo "               Set device to 'CODESYS Control Win V3'"
echo "               Add a PROFINET IO master to the 'Device' node"
echo "  4. Import the GSDML file from this repo:"
echo "               /usr/share/drive-simulator/virtualdrive.gsdml  (on RPi5)"
echo "               or copy it from: meta-drive-sim/recipes-drive/drive-simulator/files/"
echo "  5. Configure the IO device:"
echo "               Station name: 'virtualdrive' (matches DNS_CompatibleName in GSDML)"
echo "               IP address:   <RPi5 IP, e.g. 192.168.1.100>"
echo "  6. Add the VirtualDrive module in Slot 1"
echo "  7. Map I/O: GVL.StatusWord  := %IW0  (input word, byte 0)"
echo "              GVL.ActualSpeed := %ID2  (input dword, byte 2, interpret as REAL)"
echo "              GVL.Position    := %ID6  (input dword, byte 6)"
echo "              GVL.ControlWord := %QW0  (output word, byte 0)"
echo "              GVL.TargetSpeed := %QD2  (output dword, byte 2, interpret as REAL)"
echo "  8. In a CyclicTask (1ms) set:"
echo "               GVL.ControlWord := 16#000F;"
echo "               GVL.TargetSpeed := 1500.0;"
echo "  9. Download and run"
echo ""
echo "---"
echo ""

# ---------------------------------------------------------------------------
# Option B: p-net sample_app (open source IO-Controller test)
# ---------------------------------------------------------------------------
echo "--- Option B: p-net sample_app (Linux CLI tool, no account needed) ---"
echo ""

PNET_DIR="${HOME}/p-net-controller"

echo "==> Installing build dependencies"
apt-get update -qq
apt-get install -y --no-install-recommends \
    build-essential cmake git libpcap-dev python3

echo "==> Cloning p-net"
if [[ ! -d "${PNET_DIR}" ]]; then
    git clone https://github.com/rtlabs-com/p-net.git "${PNET_DIR}"
else
    echo "    Already cloned"
fi

echo "==> Building p-net sample_app"
cmake -S "${PNET_DIR}" -B "${PNET_DIR}/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF \
    -DBUILD_DOCUMENTATION=OFF
cmake --build "${PNET_DIR}/build" --target pn_dev -j"$(nproc)"

echo ""
echo "==> Setup complete (Option B)"
echo ""
echo "Usage — the sample_app acts as a simple IO-Device (not controller)."
echo "To test the RPi5 device use Wireshark to observe PROFINET frames:"
echo ""
echo "  sudo wireshark -i <interface> -f 'ether proto 0x8892' &"
echo ""
echo "To send PROFINET DCP 'Identify' and find the device:"
echo "  sudo python3 -c \""
echo "  import socket, struct"
echo "  s = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(0x8892))"
echo "  s.bind(('<interface>', 0))"
echo "  # DCP Identify All broadcast"
echo "  dcp = bytes.fromhex('ffffffffffff' + 'aabbccddeeff' + '8892' +"
echo "                       'fefe' + '0501' + '0001' + '0000' + '0004' +"
echo "                       '0002' + '00ff' + '0000')"
echo "  s.send(dcp)"
echo "  data, addr = s.recvfrom(1500)"
echo "  print('Response from:', addr)"
echo "\""
