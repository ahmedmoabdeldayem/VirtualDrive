#include "PROFINETAdapter.hpp"

#include <cstring>
#include <iostream>
#include <pthread.h>

// p-net headers — paths resolved by CMake include directories
extern "C" {
#include "pnet_api.h"
#include "osal.h"
}

PROFINETAdapter* PROFINETAdapter::instance_ = nullptr;

// ---------------------------------------------------------------------------
// p-net C callback shims — delegate to the singleton
// ---------------------------------------------------------------------------

extern "C" {

static int pnet_cb_connect(pnet_t* /*net*/, void* /*arg*/,
                            uint32_t arep, pnet_result_t* /*result*/) {
    if (PROFINETAdapter::instance()) {
        return PROFINETAdapter::instance()->onConnect(arep);
    }
    return 0;
}

static int pnet_cb_release(pnet_t* /*net*/, void* /*arg*/,
                            uint32_t arep, pnet_result_t* /*result*/) {
    if (PROFINETAdapter::instance()) {
        return PROFINETAdapter::instance()->onRelease(arep);
    }
    return 0;
}

static int pnet_cb_write(pnet_t* /*net*/, void* /*arg*/,
                          uint32_t arep, uint32_t /*api*/, uint16_t slot,
                          uint16_t subslot, uint16_t idx, uint16_t seq,
                          uint16_t len, const uint8_t* data,
                          pnet_result_t* /*result*/) {
    (void)seq;
    if (PROFINETAdapter::instance()) {
        return PROFINETAdapter::instance()->onWrite(arep, slot, subslot, idx, len, data);
    }
    return 0;
}

static int pnet_cb_read(pnet_t* /*net*/, void* /*arg*/,
                         uint32_t arep, uint32_t /*api*/, uint16_t slot,
                         uint16_t subslot, uint16_t idx, uint16_t seq,
                         uint8_t** pp_data, uint16_t* p_len,
                         pnet_result_t* /*result*/) {
    (void)seq;
    if (PROFINETAdapter::instance()) {
        return PROFINETAdapter::instance()->onRead(arep, slot, subslot, idx, pp_data, p_len);
    }
    return 0;
}

static void pnet_cb_state(pnet_t* /*net*/, void* /*arg*/,
                           uint32_t arep, pnet_event_values_t event) {
    if (PROFINETAdapter::instance()) {
        PROFINETAdapter::instance()->onState(arep, static_cast<int>(event));
    }
}

static int pnet_cb_alarm_cnf(pnet_t* /*net*/, void* /*arg*/,
                              uint32_t /*arep*/,
                              const pnet_pnio_status_t* /*status*/) {
    return 0;
}

static int pnet_cb_alarm_ack_cnf(pnet_t* /*net*/, void* /*arg*/,
                                  uint32_t /*arep*/, int /*res*/) {
    return 0;
}

static int pnet_cb_reset(pnet_t* /*net*/, void* /*arg*/,
                          bool /*should_reset_application*/,
                          uint16_t /*reset_mode*/) {
    return 0;
}

static int pnet_cb_signal_led(pnet_t* /*net*/, void* /*arg*/, bool /*led_state*/) {
    return 0;
}

}  // extern "C"

// ---------------------------------------------------------------------------
// PROFINETAdapter
// ---------------------------------------------------------------------------

PROFINETAdapter::PROFINETAdapter() {
    instance_ = this;
}

PROFINETAdapter::~PROFINETAdapter() {
    stop();
    instance_ = nullptr;
}

PROFINETAdapter* PROFINETAdapter::instance() {
    return instance_;
}

bool PROFINETAdapter::initialize(const std::string& interface) {
    interface_ = interface;

    pnet_cfg_t cfg{};
    std::memset(&cfg, 0, sizeof(cfg));

    // Device identity (must match GSDML file)
    cfg.device_id.vendor_id_hi  = 0x12;
    cfg.device_id.vendor_id_lo  = 0x34;
    cfg.device_id.device_id_hi  = 0x00;
    cfg.device_id.device_id_lo  = 0x01;
    cfg.oem_device_id.vendor_id_hi = 0x00;
    cfg.oem_device_id.vendor_id_lo = 0x00;
    cfg.oem_device_id.device_id_hi = 0x00;
    cfg.oem_device_id.device_id_lo = 0x00;

    std::strncpy(cfg.station_name,     "virtualdrive",  sizeof(cfg.station_name) - 1);
    std::strncpy(cfg.product_name,     "VirtualDrive",  sizeof(cfg.product_name) - 1);
    std::strncpy(cfg.manufacturer_name,"VD Industries", sizeof(cfg.manufacturer_name) - 1);

    cfg.send_hello              = true;
    cfg.cyclic_data_interval_us = 1000;  // 1 ms

    cfg.cb_args      = nullptr;
    cfg.connect_cb   = pnet_cb_connect;
    cfg.release_cb   = pnet_cb_release;
    cfg.write_cb     = pnet_cb_write;
    cfg.read_cb      = pnet_cb_read;
    cfg.state_cb     = pnet_cb_state;
    cfg.alarm_notify_cnf_cb  = pnet_cb_alarm_cnf;
    cfg.alarm_ack_cnf_cb     = pnet_cb_alarm_ack_cnf;
    cfg.reset_cb             = pnet_cb_reset;
    cfg.signal_led_cb        = pnet_cb_signal_led;

    cfg.num_ports = 1;
    std::strncpy(cfg.eth_interface, interface_.c_str(), sizeof(cfg.eth_interface) - 1);

    pnet_ = pnet_init(&cfg);
    if (!pnet_) {
        std::cerr << "[PROFINET] pnet_init failed\n";
        return false;
    }

    // Register DAP (slot 0, subslot 1 and port subslots)
    pnet_plug_dap(static_cast<pnet_t*>(pnet_), nullptr, 0, 1,
                  PNET_MOD_DAP_IDENT, PNET_SUBMOD_DAP_IDENT,
                  PNET_DIR_NO_IO, 0, 0, nullptr, nullptr);

    // Register application module (slot 1, subslot 1)
    pnet_plug_module(static_cast<pnet_t*>(pnet_), nullptr, 0, 1, 0x00000002U);
    pnet_plug_submodule(static_cast<pnet_t*>(pnet_), nullptr, 0, 1, 1,
                        0x00000002U, 0x00000001U,
                        PNET_DIR_IO, 10, 6);  // 10 bytes in, 6 bytes out

    std::cout << "[PROFINET] Initialized on " << interface << "\n";
    return true;
}

void PROFINETAdapter::start() {
    running_.store(true, std::memory_order_release);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    struct sched_param sp{};
    sp.sched_priority = 79;
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    pthread_attr_setschedparam(&attr, &sp);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_create(&stackThread_, &attr, &PROFINETAdapter::stackThreadEntry, this);
    pthread_attr_destroy(&attr);
}

void PROFINETAdapter::stop() {
    running_.store(false, std::memory_order_release);
    if (stackThread_) {
        pthread_join(stackThread_, nullptr);
        stackThread_ = 0;
    }
    if (pnet_) {
        pnet_exit(static_cast<pnet_t*>(pnet_));
        pnet_ = nullptr;
    }
}

void* PROFINETAdapter::stackThreadEntry(void* arg) {
    static_cast<PROFINETAdapter*>(arg)->runStack();
    return nullptr;
}

void PROFINETAdapter::runStack() {
    pthread_setname_np(pthread_self(), "pnet-stack");

    while (running_.load(std::memory_order_acquire)) {
        pnet_handle_periodic(static_cast<pnet_t*>(pnet_));
        checkTemperatureAlarm();

        // Push updated input data to p-net every cycle
        if (arep_ != 0xFFFFFFFFU) {
            uint8_t inData[10];
            uint16_t sw  = inStatusWord_.load(std::memory_order_acquire);
            float    asp = inActualSpeed_.load(std::memory_order_acquire);
            int32_t  pos = inPosition_.load(std::memory_order_acquire);

            std::memcpy(&inData[0], &sw,  2);
            std::memcpy(&inData[2], &asp, 4);
            std::memcpy(&inData[6], &pos, 4);

            pnet_input_set_data_and_iops(static_cast<pnet_t*>(pnet_),
                                         0, 1, 1,
                                         inData, sizeof(inData),
                                         PNET_IOXS_GOOD);
        }

        struct timespec ts{0, 500000};  // 0.5 ms sleep — run at 2x cyclic rate
        nanosleep(&ts, nullptr);
    }
}

void PROFINETAdapter::checkTemperatureAlarm() {
    const float temp = temperature_.load(std::memory_order_acquire);
    if (temp > 80.0f && !alarmPending_ && arep_ != 0xFFFFFFFFU) {
        alarmPending_ = true;
        pnet_alarm_send_process_alarm(static_cast<pnet_t*>(pnet_),
                                      arep_, 0, 1, 1,
                                      0x0001,  // alarm type: process alarm
                                      0,        // usi
                                      0, nullptr);
        std::cout << "[PROFINET] Temperature alarm sent (" << temp << " °C)\n";
    }
    if (temp <= 75.0f) {
        alarmPending_ = false;
    }
}

int PROFINETAdapter::onConnect(uint32_t arep) {
    arep_ = arep;
    std::cout << "[PROFINET] Connect AR=" << arep << "\n";
    return 0;
}

int PROFINETAdapter::onRelease(uint32_t arep) {
    (void)arep;
    arep_ = 0xFFFFFFFFU;
    std::cout << "[PROFINET] Release AR=" << arep << "\n";
    return 0;
}

int PROFINETAdapter::onWrite(uint32_t /*arep*/, uint16_t /*slot*/, uint16_t /*subslot*/,
                              uint16_t /*idx*/, uint16_t len, const uint8_t* data) {
    if (len < 6) return -1;

    uint16_t cw{};
    float    ts{};
    std::memcpy(&cw, &data[0], 2);
    std::memcpy(&ts, &data[2], 4);
    outControlWord_.store(cw, std::memory_order_release);
    outTargetSpeed_.store(ts, std::memory_order_release);
    return 0;
}

int PROFINETAdapter::onRead(uint32_t /*arep*/, uint16_t /*slot*/, uint16_t /*subslot*/,
                             uint16_t /*idx*/, uint8_t** /*pp_data*/, uint16_t* /*p_len*/) {
    // Acyclic read not used; input data pushed via pnet_input_set_data_and_iops
    return 0;
}

void PROFINETAdapter::onState(uint32_t /*arep*/, int event) {
    std::cout << "[PROFINET] State event=" << event << "\n";
}

uint16_t PROFINETAdapter::getControlWord()   const { return outControlWord_.load(std::memory_order_acquire); }
float    PROFINETAdapter::getTargetSpeed()   const { return outTargetSpeed_.load(std::memory_order_acquire); }
void     PROFINETAdapter::setStatusWord(uint16_t sw)  { inStatusWord_.store(sw, std::memory_order_release); }
void     PROFINETAdapter::setActualSpeed(float rpm)   { inActualSpeed_.store(rpm, std::memory_order_release); }
void     PROFINETAdapter::setActualPosition(int32_t p){ inPosition_.store(p, std::memory_order_release); }
void     PROFINETAdapter::setTorque(float /*t*/)      { /* torque not in PROFINET input payload */ }
void     PROFINETAdapter::setTemperature(float c)     { temperature_.store(c, std::memory_order_release); }
