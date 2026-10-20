#include "EtherCATAdapter.hpp"
#include <cstring>
#include <iostream>

EtherCATAdapter* EtherCATAdapter::instance_ = nullptr;

EtherCATAdapter::EtherCATAdapter()  { instance_ = this; }
EtherCATAdapter::~EtherCATAdapter() { stop(); instance_ = nullptr; }
EtherCATAdapter* EtherCATAdapter::instance() { return instance_; }

// ============================================================
#ifdef HAVE_SOES
// ============================================================
// Full SOES integration

extern "C" {
#include "soes/esc.h"
#include "soes/esc_coe.h"
#include "soes/ecat_slv.h"

/* Minimal EtherCAT object dictionary — SOES requires this symbol to be
 * defined by the application. An empty list is sufficient for simulation. */
const _objectlist SDOobjects[] = {
    {0xFFFF, 0, 0, 0, NULL, NULL}
};
}

#pragma pack(push, 1)
struct RxPDO { uint16_t control_word; int32_t target_velocity; };
struct TxPDO { uint16_t status_word;  int32_t actual_velocity; int32_t actual_position; int16_t torque; };
#pragma pack(pop)

static uint8_t rxpdo_buf[sizeof(RxPDO)]{};
static uint8_t txpdo_buf[sizeof(TxPDO)]{};

extern "C" {
    void cb_set_outputs(void) {
        if (EtherCATAdapter::instance()) EtherCATAdapter::instance()->onSetOutputs();
    }
    void cb_get_inputs(void) {
        if (EtherCATAdapter::instance()) EtherCATAdapter::instance()->onGetInputs();
    }
    void cb_state_change(uint8_t ns, uint8_t os) {
        if (EtherCATAdapter::instance()) EtherCATAdapter::instance()->onStateChange(ns, os);
    }
}

bool EtherCATAdapter::initialize(const std::string& interface) {
    interface_ = interface;
    std::cout << "[EtherCAT] SOES on " << interface << "\n";
    return true;
}

void* EtherCATAdapter::stackThreadEntry(void* arg) {
    static_cast<EtherCATAdapter*>(arg)->runStack();
    return nullptr;
}

void EtherCATAdapter::runStack() {
    pthread_setname_np(pthread_self(), "soes-stack");
    esc_cfg_t cfg{};
    cfg.rxpdo_override = cb_set_outputs;
    cfg.txpdo_override = cb_get_inputs;
    ecat_slv_init(&cfg);
    while (running_.load(std::memory_order_acquire))
        ecat_slv_poll();
}

void EtherCATAdapter::onSetOutputs() {
    RxPDO pdo{};
    std::memcpy(&pdo, rxpdo_buf, sizeof(pdo));
    rxControlWord_.store(pdo.control_word,  std::memory_order_release);
    rxTargetVel_.store(pdo.target_velocity, std::memory_order_release);
}

void EtherCATAdapter::onGetInputs() {
    TxPDO pdo{};
    pdo.status_word     = txStatusWord_.load(std::memory_order_acquire);
    pdo.actual_velocity = txActualVel_.load(std::memory_order_acquire);
    pdo.actual_position = txActualPos_.load(std::memory_order_acquire);
    pdo.torque          = txTorque_.load(std::memory_order_acquire);
    std::memcpy(txpdo_buf, &pdo, sizeof(pdo));
}

void EtherCATAdapter::onStateChange(uint8_t ns, uint8_t os) {
    std::cout << "[EtherCAT] ESM " << (int)os << " -> " << (int)ns << "\n";
}

// ============================================================
#else  // HAVE_SOES — stub implementation
// ============================================================

bool EtherCATAdapter::initialize(const std::string& interface) {
    interface_ = interface;
    std::cout << "[EtherCAT] stub mode — SOES not compiled in\n";
    return true;
}

void* EtherCATAdapter::stackThreadEntry(void* /*arg*/) { return nullptr; }
void  EtherCATAdapter::runStack() {}
void  EtherCATAdapter::onSetOutputs() {}
void  EtherCATAdapter::onGetInputs() {}
void  EtherCATAdapter::onStateChange(uint8_t, uint8_t) {}

#endif  // HAVE_SOES

// ============================================================
// Common regardless of SOES presence
// ============================================================

void EtherCATAdapter::start() {
    running_.store(true, std::memory_order_release);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    struct sched_param sp{}; sp.sched_priority = 80;
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    pthread_attr_setschedparam(&attr, &sp);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_create(&stackThread_, &attr, &EtherCATAdapter::stackThreadEntry, this);
    pthread_attr_destroy(&attr);
}

void EtherCATAdapter::stop() {
    running_.store(false, std::memory_order_release);
    if (stackThread_) { pthread_join(stackThread_, nullptr); stackThread_ = 0; }
}

uint16_t EtherCATAdapter::getControlWord()   const { return rxControlWord_.load(std::memory_order_acquire); }
float    EtherCATAdapter::getTargetSpeed()   const { return static_cast<float>(rxTargetVel_.load(std::memory_order_acquire)) / 100.0f; }
void     EtherCATAdapter::setStatusWord(uint16_t sw)  { txStatusWord_.store(sw, std::memory_order_release); }
void     EtherCATAdapter::setActualSpeed(float rpm)   { txActualVel_.store(static_cast<int32_t>(rpm * 100.0f), std::memory_order_release); }
void     EtherCATAdapter::setActualPosition(int32_t p){ txActualPos_.store(p, std::memory_order_release); }
void     EtherCATAdapter::setTorque(float t)          { txTorque_.store(static_cast<int16_t>(t * 10.0f), std::memory_order_release); }
