#!/usr/bin/env bash
# Setup script for PC-side EtherCAT master (SOEM + simple test master)
# Run as: sudo bash setup_ethercat_master.sh <ethernet-interface>
# Example: sudo bash setup_ethercat_master.sh enp3s0
set -euo pipefail

IFACE="${1:-}"
if [[ -z "${IFACE}" ]]; then
    echo "Usage: $0 <ethernet-interface>"
    echo "  Example: $0 enp3s0"
    exit 1
fi

INSTALL_DIR="${HOME}/soem-test"
SOEM_DIR="${INSTALL_DIR}/SOEM"

echo "==> Installing build dependencies"
apt-get update -qq
apt-get install -y --no-install-recommends \
    build-essential cmake git libpcap-dev

echo "==> Cloning SOEM"
mkdir -p "${INSTALL_DIR}"
if [[ ! -d "${SOEM_DIR}" ]]; then
    git clone https://github.com/OpenEtherCATsociety/SOEM.git "${SOEM_DIR}"
else
    echo "    SOEM already cloned; skipping"
fi

echo "==> Building SOEM"
cmake -S "${SOEM_DIR}" -B "${SOEM_DIR}/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "${SOEM_DIR}/build" -j"$(nproc)"

echo "==> Setting ${IFACE} to promiscuous mode (required for raw EtherCAT frames)"
ip link set dev "${IFACE}" promisc on
ip link set dev "${IFACE}" up

echo "==> Generating simple C EtherCAT test master"
TEST_MASTER="${INSTALL_DIR}/vd_master.c"
cat > "${TEST_MASTER}" << 'CEOF'
/* VirtualDrive EtherCAT Test Master
 * Scans for slaves, moves to OPERATIONAL, then runs a 1 ms cycle:
 *   - Writes ControlWord=0x000F (Enable Operation) + TargetVelocity=150000 (1500 RPM)
 *   - Reads back StatusWord and ActualVelocity
 * Compile: gcc -o vd_master vd_master.c -L<SOEM_BUILD>/lib -lsoem -lpthread
 *          (adjust SOEM include/lib paths as needed)
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include "ethercat.h"

#pragma pack(push,1)
typedef struct {
    uint16_t control_word;
    int32_t  target_velocity;   /* RPM * 100 */
} RxPDO;

typedef struct {
    uint16_t status_word;
    int32_t  actual_velocity;   /* RPM * 100 */
    int32_t  actual_position;
    int16_t  torque;
} TxPDO;
#pragma pack(pop)

static volatile int g_run = 1;
static void on_signal(int s) { (void)s; g_run = 0; }

int main(int argc, char *argv[])
{
    const char *iface = (argc > 1) ? argv[1] : "eth0";

    printf("[Master] Initializing EtherCAT on %s\n", iface);
    if (!ec_init(iface)) {
        fprintf(stderr, "[Master] ec_init failed\n");
        return 1;
    }

    if (ec_config_init(FALSE) <= 0) {
        fprintf(stderr, "[Master] No slaves found\n");
        ec_close();
        return 1;
    }
    printf("[Master] %d slave(s) found\n", ec_slavecount);

    ec_config_map(NULL);
    ec_configdc();

    printf("[Master] Moving slaves to SAFE_OP\n");
    ec_statecheck(0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 4);

    ec_slave[0].state = EC_STATE_OPERATIONAL;
    ec_send_processdata();
    ec_receive_processdata(EC_TIMEOUTRET);
    ec_writestate(0);
    ec_statecheck(0, EC_STATE_OPERATIONAL, EC_TIMEOUTSTATE);

    if (ec_slave[0].state != EC_STATE_OPERATIONAL) {
        fprintf(stderr, "[Master] Slave did not reach OPERATIONAL\n");
        ec_close();
        return 1;
    }
    printf("[Master] Slave in OPERATIONAL — starting 1ms cycle\n");

    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);

    while (g_run) {
        next.tv_nsec += 1000000L;   /* 1 ms */
        if (next.tv_nsec >= 1000000000L) {
            next.tv_nsec -= 1000000000L;
            next.tv_sec++;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);

        ec_send_processdata();
        ec_receive_processdata(EC_TIMEOUTRET);

        /* Map outputs to first slave's process data image */
        RxPDO *rx = (RxPDO *)ec_slave[1].outputs;
        rx->control_word    = 0x000F;   /* Enable Operation */
        rx->target_velocity = 150000;   /* 1500 RPM */

        /* Map inputs */
        TxPDO *tx = (TxPDO *)ec_slave[1].inputs;
        printf("[Master] SW=0x%04X  vel=%d RPM  pos=%d\n",
               tx->status_word,
               tx->actual_velocity / 100,
               tx->actual_position);
    }

    printf("[Master] Stopping\n");
    ec_slave[0].state = EC_STATE_INIT;
    ec_writestate(0);
    ec_close();
    return 0;
}
CEOF

echo "==> Compiling test master"
SOEM_INC="${SOEM_DIR}/SOEM"
SOEM_LIB="${SOEM_DIR}/build/lib"

gcc -O2 -Wall -I"${SOEM_INC}" \
    -o "${INSTALL_DIR}/vd_master" "${TEST_MASTER}" \
    -L"${SOEM_LIB}" -lsoem -lpthread 2>/dev/null || \
gcc -O2 -Wall -I"${SOEM_DIR}" -I"${SOEM_DIR}/osal" -I"${SOEM_DIR}/osal/linux" \
    -I"${SOEM_DIR}/oshw/linux" \
    -o "${INSTALL_DIR}/vd_master" "${TEST_MASTER}" \
    -L"${SOEM_DIR}/build/libsoem.a" "${SOEM_DIR}/build/libsoem.a" -lpthread || \
echo "NOTE: Auto-compilation failed — compile manually with the SOEM include paths."

echo ""
echo "==> Setup complete"
echo ""
echo "Usage (on this PC, connected to RPi5 via ${IFACE}):"
echo "  sudo ${INSTALL_DIR}/vd_master ${IFACE}"
echo ""
echo "The RPi5 must be running:"
echo "  drive-simulator --protocol ethercat --interface eth0"
