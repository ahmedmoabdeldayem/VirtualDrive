#include "PROFINETAdapter.hpp"
#include <cstring>
#include <iostream>

PROFINETAdapter* PROFINETAdapter::instance_ = nullptr;

PROFINETAdapter::PROFINETAdapter()  { instance_ = this; }
PROFINETAdapter::~PROFINETAdapter() { stop(); instance_ = nullptr; }
PROFINETAdapter* PROFINETAdapter::instance() { return instance_; }

// ============================================================
#ifdef HAVE_PNET
// ============================================================

extern "C" {
#include "pnet_api.h"
#include "osal.h"
}

extern "C" {
static int cb_connect(pnet_t* /*n*/, void* /*a*/, uint32_t arep, pnet_result_t* /*r*/) {
    return PROFINETAdapter::instance() ? PROFINETAdapter::instance()->onConnect(arep) : 0;
}
static int cb_release(pnet_t* /*n*/, void* /*a*/, uint32_t arep, pnet_result_t* /*r*/) {
    return PROFINETAdapter::instance() ? PROFINETAdapter::instance()->onRelease(arep) : 0;
}
static int cb_dcontrol(pnet_t*,void*,uint32_t,pnet_control_command_t,pnet_result_t*) { return 0; }
static int cb_ccontrol(pnet_t*,void*,uint32_t,pnet_result_t*) { return 0; }
static int cb_write(pnet_t* /*n*/, void* /*a*/, uint32_t arep, uint32_t /*api*/,
                    uint16_t slot, uint16_t sub, uint16_t idx, uint16_t seq,
                    uint16_t len, const uint8_t* data, pnet_result_t* /*r*/) {
    (void)seq;
    return PROFINETAdapter::instance()
        ? PROFINETAdapter::instance()->onWrite(arep, slot, sub, idx, len, data) : 0;
}
static int cb_read(pnet_t* /*n*/, void* /*a*/, uint32_t arep, uint32_t /*api*/,
                   uint16_t slot, uint16_t sub, uint16_t idx, uint16_t seq,
                   uint8_t** pp, uint16_t* pl, pnet_result_t* /*r*/) {
    (void)seq;
    return PROFINETAdapter::instance()
        ? PROFINETAdapter::instance()->onRead(arep, slot, sub, idx, pp, pl) : 0;
}
static int cb_exp_module(pnet_t*,void*,uint32_t,uint32_t,uint16_t,uint32_t,uint32_t) { return 0; }
static int cb_exp_submodule(pnet_t*,void*,uint32_t,uint32_t,uint16_t,uint16_t,
                             uint32_t,uint32_t,const pnet_data_cfg_t*) { return 0; }
static int cb_new_data_status(pnet_t*,void*,uint32_t,uint32_t,uint8_t) { return 0; }
static int cb_alarm_ind(pnet_t*,void*,uint32_t,uint32_t,uint16_t,uint16_t,
                        uint16_t,uint16_t,const pnet_alarm_argument_t*,
                        uint16_t,uint16_t,const uint8_t*) { return 0; }
static void cb_state(pnet_t* /*n*/, void* /*a*/, uint32_t arep, pnet_event_values_t ev) {
    if (PROFINETAdapter::instance())
        PROFINETAdapter::instance()->onState(arep, static_cast<int>(ev));
}
static int cb_alarm_cnf(pnet_t*,void*,uint32_t,const pnet_pnio_status_t*) { return 0; }
static int cb_alarm_ack(pnet_t*,void*,uint32_t,int) { return 0; }
static int cb_reset(pnet_t*,void*,bool,uint16_t) { return 0; }
static int cb_led(pnet_t*,void*,bool) { return 0; }
static int cb_sm_released(pnet_t*,void*,uint32_t,uint32_t,uint16_t,uint16_t,pnet_result_t*) { return 0; }
}

bool PROFINETAdapter::initialize(const std::string& interface) {
    interface_ = interface;
    pnet_cfg_t cfg{};
    std::memset(&cfg, 0, sizeof(cfg));
    cfg.tick_us                = 1000;
    cfg.device_id.vendor_id_hi = 0x12; cfg.device_id.vendor_id_lo = 0x34;
    cfg.device_id.device_id_hi = 0x00; cfg.device_id.device_id_lo = 0x01;
    std::strncpy(cfg.station_name, "virtualdrive", sizeof(cfg.station_name)-1);
    cfg.send_hello             = true;
    cfg.num_physical_ports     = 1;
    cfg.cb_arg                 = nullptr;
    cfg.state_cb               = cb_state;
    cfg.connect_cb             = cb_connect;
    cfg.release_cb             = cb_release;
    cfg.dcontrol_cb            = cb_dcontrol;
    cfg.ccontrol_cb            = cb_ccontrol;
    cfg.write_cb               = cb_write;
    cfg.read_cb                = cb_read;
    cfg.exp_module_cb          = cb_exp_module;
    cfg.exp_submodule_cb       = cb_exp_submodule;
    cfg.new_data_status_cb     = cb_new_data_status;
    cfg.alarm_ind_cb           = cb_alarm_ind;
    cfg.alarm_cnf_cb           = cb_alarm_cnf;
    cfg.alarm_ack_cnf_cb       = cb_alarm_ack;
    cfg.reset_cb               = cb_reset;
    cfg.signal_led_cb          = cb_led;
    cfg.sm_released_cb         = cb_sm_released;
    cfg.if_cfg.main_netif_name = interface_.c_str();
    cfg.pnal_cfg.bg_worker_thread.prio       = 10;
    cfg.pnal_cfg.bg_worker_thread.stack_size = 4096;
    cfg.pnal_cfg.eth_recv_thread.prio        = 11;
    cfg.pnal_cfg.eth_recv_thread.stack_size  = 4096;
    cfg.pnal_cfg.main_thread.prio            = 9;
    cfg.pnal_cfg.main_thread.stack_size      = 4096;
    pnet_ = pnet_init(&cfg);
    if (!pnet_) { std::cerr << "[PROFINET] pnet_init failed\n"; return false; }
    pnet_plug_module(static_cast<pnet_t*>(pnet_), 0, 1, 0x00000002U);
    pnet_plug_submodule(static_cast<pnet_t*>(pnet_), 0, 1, 1,
                        0x00000002U, 0x00000001U, PNET_DIR_IO, 10, 6);
    std::cout << "[PROFINET] p-net on " << interface << "\n";
    return true;
}

void* PROFINETAdapter::stackThreadEntry(void* arg) {
    static_cast<PROFINETAdapter*>(arg)->runStack(); return nullptr;
}

void PROFINETAdapter::runStack() {
    pthread_setname_np(pthread_self(), "pnet-stack");
    while (running_.load(std::memory_order_acquire)) {
        pnet_handle_periodic(static_cast<pnet_t*>(pnet_));
        checkTemperatureAlarm();
        if (arep_ != 0xFFFFFFFFU) {
            uint8_t d[10];
            uint16_t sw  = inStatusWord_.load(std::memory_order_acquire);
            float    asp = inActualSpeed_.load(std::memory_order_acquire);
            int32_t  pos = inPosition_.load(std::memory_order_acquire);
            std::memcpy(&d[0], &sw, 2); std::memcpy(&d[2], &asp, 4); std::memcpy(&d[6], &pos, 4);
            pnet_input_set_data_and_iops(static_cast<pnet_t*>(pnet_), 0, 1, 1,
                                         d, sizeof(d), PNET_IOXS_GOOD);
        }
        struct timespec ts{0, 500000}; nanosleep(&ts, nullptr);
    }
}

void PROFINETAdapter::checkTemperatureAlarm() {
    const float t = temperature_.load(std::memory_order_acquire);
    if (t > 80.0f && !alarmPending_ && arep_ != 0xFFFFFFFFU) {
        alarmPending_ = true;
        pnet_alarm_send_process_alarm(static_cast<pnet_t*>(pnet_),
                                      arep_, 0, 1, 1, 0x0001, 0, 0, nullptr);
        std::cout << "[PROFINET] Temp alarm " << t << "°C\n";
    }
    if (t <= 75.0f) alarmPending_ = false;
}

int PROFINETAdapter::onConnect(uint32_t arep) {
    arep_ = arep; std::cout << "[PROFINET] Connect AR=" << arep << "\n"; return 0;
}
int PROFINETAdapter::onRelease(uint32_t arep) {
    (void)arep; arep_ = 0xFFFFFFFFU; return 0;
}
int PROFINETAdapter::onWrite(uint32_t, uint16_t, uint16_t, uint16_t,
                              uint16_t len, const uint8_t* data) {
    if (len < 6) return -1;
    uint16_t cw{}; float ts{};
    std::memcpy(&cw, &data[0], 2); std::memcpy(&ts, &data[2], 4);
    outControlWord_.store(cw, std::memory_order_release);
    outTargetSpeed_.store(ts, std::memory_order_release);
    return 0;
}
int PROFINETAdapter::onRead(uint32_t,uint16_t,uint16_t,uint16_t,uint8_t**,uint16_t*) { return 0; }
void PROFINETAdapter::onState(uint32_t, int e) { std::cout << "[PROFINET] event=" << e << "\n"; }

// ============================================================
#else  // HAVE_PNET — stub
// ============================================================

bool PROFINETAdapter::initialize(const std::string& interface) {
    interface_ = interface;
    std::cout << "[PROFINET] stub mode — p-net not compiled in\n";
    return true;
}
void* PROFINETAdapter::stackThreadEntry(void* /*a*/) { return nullptr; }
void  PROFINETAdapter::runStack() {}
void  PROFINETAdapter::checkTemperatureAlarm() {}
int   PROFINETAdapter::onConnect(uint32_t)  { return 0; }
int   PROFINETAdapter::onRelease(uint32_t)  { return 0; }
int   PROFINETAdapter::onWrite(uint32_t,uint16_t,uint16_t,uint16_t,uint16_t,const uint8_t*) { return 0; }
int   PROFINETAdapter::onRead(uint32_t,uint16_t,uint16_t,uint16_t,uint8_t**,uint16_t*) { return 0; }
void  PROFINETAdapter::onState(uint32_t,int) {}

#endif  // HAVE_PNET

// ============================================================
// Common
// ============================================================

void PROFINETAdapter::start() {
    running_.store(true, std::memory_order_release);
    pthread_attr_t attr; pthread_attr_init(&attr);
    struct sched_param sp{}; sp.sched_priority = 79;
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    pthread_attr_setschedparam(&attr, &sp);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_create(&stackThread_, &attr, &PROFINETAdapter::stackThreadEntry, this);
    pthread_attr_destroy(&attr);
}

void PROFINETAdapter::stop() {
    running_.store(false, std::memory_order_release);
    if (stackThread_) { pthread_join(stackThread_, nullptr); stackThread_ = 0; }
#ifdef HAVE_PNET
    if (pnet_) { pnet_exit(static_cast<pnet_t*>(pnet_)); pnet_ = nullptr; }
#endif
}

uint16_t PROFINETAdapter::getControlWord()   const { return outControlWord_.load(std::memory_order_acquire); }
float    PROFINETAdapter::getTargetSpeed()   const { return outTargetSpeed_.load(std::memory_order_acquire); }
void     PROFINETAdapter::setStatusWord(uint16_t sw)  { inStatusWord_.store(sw, std::memory_order_release); }
void     PROFINETAdapter::setActualSpeed(float rpm)   { inActualSpeed_.store(rpm, std::memory_order_release); }
void     PROFINETAdapter::setActualPosition(int32_t p){ inPosition_.store(p, std::memory_order_release); }
void     PROFINETAdapter::setTorque(float /*t*/)      {}
void     PROFINETAdapter::setTemperature(float c)     { temperature_.store(c, std::memory_order_release); }
