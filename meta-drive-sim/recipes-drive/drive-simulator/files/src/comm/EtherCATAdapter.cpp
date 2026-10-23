#include "EtherCATAdapter.hpp"

#include <cstring>
#include <iostream>
#include <pthread.h>

// SOES public headers.  Paths resolved by CMake include directories.
extern "C" {
#include "soes/esc.h"
#include "soes/esc_coe.h"
#include "soes/esc_eoe.h"
}

// ---------------------------------------------------------------------------
// Process data layout in SOES process image
// ---------------------------------------------------------------------------

#pragma pack(push, 1)
struct RxPDO {
    uint16_t control_word;    // 0x6040
    int32_t  target_velocity; // 0x6042 (RPM * 100)
};

struct TxPDO {
    uint16_t status_word;     // 0x6041
    int32_t  actual_velocity; // 0x606C (RPM * 100)
    int32_t  actual_position; // 0x6064
    int16_t  torque;          // 0x6077 (0.1% of rated)
};
#pragma pack(pop)

// SOES links these by symbol name from objectlist.c
extern uint8_t rxpdo_buffer[sizeof(RxPDO)];
extern uint8_t txpdo_buffer[sizeof(TxPDO)];

uint8_t rxpdo_buffer[sizeof(RxPDO)]{};
uint8_t txpdo_buffer[sizeof(TxPDO)]{};

// ---------------------------------------------------------------------------
// SOES application callbacks — C linkage, dispatch to singleton
// ---------------------------------------------------------------------------

EtherCATAdapter* EtherCATAdapter::instance_ = nullptr;

extern "C" {

void cb_set_outputs(void) {
    if (EtherCATAdapter::instance()) {
        EtherCATAdapter::instance()->onSetOutputs();
    }
}

void cb_get_inputs(void) {
    if (EtherCATAdapter::instance()) {
        EtherCATAdapter::instance()->onGetInputs();
    }
}

void cb_state_change(uint8_t new_state, uint8_t old_state) {
    if (EtherCATAdapter::instance()) {
        EtherCATAdapter::instance()->onStateChange(new_state, old_state);
    }
}

}  // extern "C"

// ---------------------------------------------------------------------------
// SOES object dictionary definitions (CoE, CiA 402 profile objects)
// ---------------------------------------------------------------------------
// Normally in a separate objectlist.c, but inlined here for a self-contained
// translation unit.  Only the PDO mapping and identity objects are defined.
extern "C" {

#include "soes/objectlist.h"

// Object 0x1000: Device Type — 0x00020192 = CiA 402 drives profile
static uint32_t obj1000 = 0x00020192U;

// Object 0x1018: Identity Object (sub 0=4, sub 1=VendorID, sub 2=ProductCode)
static uint8_t  obj1018_00 = 4;
static uint32_t obj1018_01 = 0x00000000U;  // Vendor ID (0 = unregistered)
static uint32_t obj1018_02 = 0x00000001U;  // Product code
static uint32_t obj1018_03 = 0x00000001U;  // Revision
static uint32_t obj1018_04 = 0x00000000U;  // Serial

// Object 0x1600: RxPDO Mapping
static uint8_t  obj1600_00 = 2;
static uint32_t obj1600_01 = 0x60400010U;  // ControlWord    UINT16 → bit[31:16]=0x6040, bit[15:8]=0x00, bit[7:0]=16
static uint32_t obj1600_02 = 0x60420020U;  // TargetVelocity INT32

// Object 0x1A00: TxPDO Mapping
static uint8_t  obj1a00_00 = 4;
static uint32_t obj1a00_01 = 0x60410010U;  // StatusWord     UINT16
static uint32_t obj1a00_02 = 0x606C0020U;  // VelocityActual INT32
static uint32_t obj1a00_03 = 0x60640020U;  // PositionActual INT32
static uint32_t obj1a00_04 = 0x60770010U;  // TorqueActual   INT16

// Object 0x1C12: RxPDO Assign
static uint8_t  obj1c12_00 = 1;
static uint16_t obj1c12_01 = 0x1600U;

// Object 0x1C13: TxPDO Assign
static uint8_t  obj1c13_00 = 1;
static uint16_t obj1c13_01 = 0x1A00U;

// Object 0x6040: ControlWord (actual value driven from RxPDO by SOES)
static uint16_t obj6040 = 0;

// Object 0x6041: StatusWord
static uint16_t obj6041 = 0;

// Object 0x6042: Target Velocity
static int32_t obj6042 = 0;

// Object 0x606C: Velocity Actual
static int32_t obj606c = 0;

// Object 0x6064: Position Actual
static int32_t obj6064 = 0;

// Object 0x6077: Torque Actual
static int16_t obj6077 = 0;

// SOES object dictionary table (terminated by OBJNULL)
const _objd SDO1000[] = {
    {0x00, DTYPE_UNSIGNED32, 0x20, "Device Type",       &obj1000,    nullptr, nullptr}
};
const _objd SDO1018[] = {
    {0x00, DTYPE_UNSIGNED8,  0x08, "Highest Sub",       &obj1018_00, nullptr, nullptr},
    {0x01, DTYPE_UNSIGNED32, 0x20, "Vendor ID",         &obj1018_01, nullptr, nullptr},
    {0x02, DTYPE_UNSIGNED32, 0x20, "Product Code",      &obj1018_02, nullptr, nullptr},
    {0x03, DTYPE_UNSIGNED32, 0x20, "Revision",          &obj1018_03, nullptr, nullptr},
    {0x04, DTYPE_UNSIGNED32, 0x20, "Serial",            &obj1018_04, nullptr, nullptr}
};
const _objd SDO1600[] = {
    {0x00, DTYPE_UNSIGNED8,  0x08, "Num Objects",       &obj1600_00, nullptr, nullptr},
    {0x01, DTYPE_UNSIGNED32, 0x20, "Mapping 1",         &obj1600_01, nullptr, nullptr},
    {0x02, DTYPE_UNSIGNED32, 0x20, "Mapping 2",         &obj1600_02, nullptr, nullptr}
};
const _objd SDO1A00[] = {
    {0x00, DTYPE_UNSIGNED8,  0x08, "Num Objects",       &obj1a00_00, nullptr, nullptr},
    {0x01, DTYPE_UNSIGNED32, 0x20, "Mapping 1",         &obj1a00_01, nullptr, nullptr},
    {0x02, DTYPE_UNSIGNED32, 0x20, "Mapping 2",         &obj1a00_02, nullptr, nullptr},
    {0x03, DTYPE_UNSIGNED32, 0x20, "Mapping 3",         &obj1a00_03, nullptr, nullptr},
    {0x04, DTYPE_UNSIGNED32, 0x20, "Mapping 4",         &obj1a00_04, nullptr, nullptr}
};
const _objd SDO1C12[] = {
    {0x00, DTYPE_UNSIGNED8,  0x08, "RxPDO Assign Num",  &obj1c12_00, nullptr, nullptr},
    {0x01, DTYPE_UNSIGNED16, 0x10, "RxPDO Assign 1",    &obj1c12_01, nullptr, nullptr}
};
const _objd SDO1C13[] = {
    {0x00, DTYPE_UNSIGNED8,  0x08, "TxPDO Assign Num",  &obj1c13_00, nullptr, nullptr},
    {0x01, DTYPE_UNSIGNED16, 0x10, "TxPDO Assign 1",    &obj1c13_01, nullptr, nullptr}
};
const _objd SDO6040[] = {
    {0x00, DTYPE_UNSIGNED16, 0x10, "ControlWord",       &obj6040,    nullptr, nullptr}
};
const _objd SDO6041[] = {
    {0x00, DTYPE_UNSIGNED16, 0x10, "StatusWord",        &obj6041,    nullptr, nullptr}
};
const _objd SDO6042[] = {
    {0x00, DTYPE_INTEGER32,  0x20, "Target Velocity",   &obj6042,    nullptr, nullptr}
};
const _objd SDO606C[] = {
    {0x00, DTYPE_INTEGER32,  0x20, "Velocity Actual",   &obj606c,    nullptr, nullptr}
};
const _objd SDO6064[] = {
    {0x00, DTYPE_INTEGER32,  0x20, "Position Actual",   &obj6064,    nullptr, nullptr}
};
const _objd SDO6077[] = {
    {0x00, DTYPE_INTEGER16,  0x10, "Torque Actual",     &obj6077,    nullptr, nullptr}
};

// SOES master object dictionary (sorted by index, required format)
const _objectlist SDOobjects[] = {
    {0x1000, OTYPE_VAR,     0, 0, "Device Type",          SDO1000},
    {0x1018, OTYPE_RECORD,  0, 4, "Identity",              SDO1018},
    {0x1600, OTYPE_RECORD,  0, 2, "RxPDO 1 Mapping",      SDO1600},
    {0x1A00, OTYPE_RECORD,  0, 4, "TxPDO 1 Mapping",      SDO1A00},
    {0x1C12, OTYPE_ARRAY,   0, 1, "RxPDO Assign",         SDO1C12},
    {0x1C13, OTYPE_ARRAY,   0, 1, "TxPDO Assign",         SDO1C13},
    {0x6040, OTYPE_VAR,     0, 0, "ControlWord",          SDO6040},
    {0x6041, OTYPE_VAR,     0, 0, "StatusWord",           SDO6041},
    {0x6042, OTYPE_VAR,     0, 0, "Target Velocity",      SDO6042},
    {0x606C, OTYPE_VAR,     0, 0, "Velocity Actual",      SDO606C},
    {0x6064, OTYPE_VAR,     0, 0, "Position Actual",      SDO6064},
    {0x6077, OTYPE_VAR,     0, 0, "Torque Actual",        SDO6077},
    {0xFFFF, 0, 0, 0, "", nullptr}  // sentinel
};

}  // extern "C"

// ---------------------------------------------------------------------------
// EtherCATAdapter
// ---------------------------------------------------------------------------

EtherCATAdapter::EtherCATAdapter() {
    instance_ = this;
}

EtherCATAdapter::~EtherCATAdapter() {
    stop();
    instance_ = nullptr;
}

EtherCATAdapter* EtherCATAdapter::instance() {
    return instance_;
}

bool EtherCATAdapter::initialize(const std::string& interface) {
    interface_ = interface;
    std::cout << "[EtherCAT] Initialized on " << interface << "\n";
    return true;
}

void EtherCATAdapter::start() {
    running_.store(true, std::memory_order_release);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    struct sched_param sp{};
    sp.sched_priority = 80;
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    pthread_attr_setschedparam(&attr, &sp);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_create(&stackThread_, &attr, &EtherCATAdapter::stackThreadEntry, this);
    pthread_attr_destroy(&attr);
}

void EtherCATAdapter::stop() {
    running_.store(false, std::memory_order_release);
    if (stackThread_) {
        pthread_join(stackThread_, nullptr);
        stackThread_ = 0;
    }
}

void* EtherCATAdapter::stackThreadEntry(void* arg) {
    static_cast<EtherCATAdapter*>(arg)->runStack();
    return nullptr;
}

void EtherCATAdapter::runStack() {
    pthread_setname_np(pthread_self(), "soes-stack");

    esc_cfg_t cfg{};
    cfg.user_arg                = 0;
    cfg.use_interrupt           = 0;
    cfg.watchdog_cnt            = 2000;
    cfg.pre_state_change_hook   = nullptr;
    cfg.post_state_change_hook  = nullptr;
    cfg.application_hook        = nullptr;
    cfg.safeoutput_override     = nullptr;
    cfg.pre_object_download_hook= nullptr;
    cfg.post_object_download_hook= nullptr;
    cfg.rxpdo_override          = cb_set_outputs;
    cfg.txpdo_override          = cb_get_inputs;
    cfg.esc_hw_interrupt_enable = nullptr;
    cfg.esc_hw_interrupt_disable= nullptr;
    cfg.esc_hw_eep_handler      = nullptr;
    cfg.esc_check_dc_handler    = nullptr;

    // ecat_slv_init sets up the raw socket based on interface name
    // (SOES reads the interface from env var IFACE or compile-time default)
    ecat_slv_init(&cfg);

    while (running_.load(std::memory_order_acquire)) {
        ecat_slv_poll(&cfg);
    }
}

void EtherCATAdapter::onSetOutputs() {
    RxPDO pdo{};
    std::memcpy(&pdo, rxpdo_buffer, sizeof(pdo));
    rxControlWord_.store(pdo.control_word,  std::memory_order_release);
    rxTargetVel_.store(pdo.target_velocity, std::memory_order_release);
}

void EtherCATAdapter::onGetInputs() {
    TxPDO pdo{};
    pdo.status_word    = txStatusWord_.load(std::memory_order_acquire);
    pdo.actual_velocity= txActualVel_.load(std::memory_order_acquire);
    pdo.actual_position= txActualPos_.load(std::memory_order_acquire);
    pdo.torque         = txTorque_.load(std::memory_order_acquire);
    std::memcpy(txpdo_buffer, &pdo, sizeof(pdo));
}

void EtherCATAdapter::onStateChange(uint8_t newState, uint8_t oldState) {
    static const char* esm[] = {"INIT", "unknown", "PREOP", "unknown",
                                 "SAFEOP", "unknown", "unknown", "unknown", "OP"};
    const char* ns = (newState < 9) ? esm[newState] : "?";
    const char* os = (oldState < 9) ? esm[oldState] : "?";
    std::cout << "[EtherCAT] ESM: " << os << " -> " << ns << "\n";
}

uint16_t EtherCATAdapter::getControlWord() const {
    return rxControlWord_.load(std::memory_order_acquire);
}

float EtherCATAdapter::getTargetSpeed() const {
    return static_cast<float>(rxTargetVel_.load(std::memory_order_acquire)) / 100.0f;
}

void EtherCATAdapter::setStatusWord(uint16_t sw) {
    txStatusWord_.store(sw, std::memory_order_release);
}

void EtherCATAdapter::setActualSpeed(float rpm) {
    txActualVel_.store(static_cast<int32_t>(rpm * 100.0f), std::memory_order_release);
}

void EtherCATAdapter::setActualPosition(int32_t pos) {
    txActualPos_.store(pos, std::memory_order_release);
}

void EtherCATAdapter::setTorque(float torque) {
    txTorque_.store(static_cast<int16_t>(torque * 10.0f), std::memory_order_release);
}
