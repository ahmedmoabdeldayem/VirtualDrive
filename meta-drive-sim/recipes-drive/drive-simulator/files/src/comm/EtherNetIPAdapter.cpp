#include "EtherNetIPAdapter.hpp"
#include <cstring>
#include <iostream>

EtherNetIPAdapter* EtherNetIPAdapter::instance_ = nullptr;

EtherNetIPAdapter::EtherNetIPAdapter()  { instance_ = this; }
EtherNetIPAdapter::~EtherNetIPAdapter() { stop(); instance_ = nullptr; }
EtherNetIPAdapter* EtherNetIPAdapter::instance() { return instance_; }

// ============================================================
#ifdef HAVE_OPENER
// ============================================================

extern "C" {
#include "opener_api.h"
#include "cipcommon.h"
#include "cipassembly.h"
#include "networkhandler.h"
}

static constexpr EipUint32 kOutAssembly = 0x64;
static constexpr EipUint32 kInAssembly  = 0x65;
static constexpr EipUint32 kCfgAssembly = 0x66;
static constexpr uint16_t  kOutSize     = 6;
static constexpr uint16_t  kInSize      = 10;

static uint8_t s_outBuf[kOutSize]{};
static uint8_t s_inBuf[kInSize]{};
static uint8_t s_cfgBuf[4]{};

bool EtherNetIPAdapter::initialize(const std::string& interface) {
    interface_ = interface;
    CipStackInit(0xC0A80064);
    CipClass* ac = GetCipClass(kCipAssemblyClassCode);
    auto addAsm = [&](EipUint32 id, uint8_t* buf, uint16_t sz) {
        CipInstance* inst = AddCipInstance(ac, id);
        InsertAttribute(inst, 3, kCipByteArray, EncodeCipByteArray,
                        nullptr, kGetableSingleAndAll | kSetable);
        SetAssemblyInstanceData(inst, buf, sz);
    };
    addAsm(kOutAssembly, s_outBuf, kOutSize);
    addAsm(kInAssembly,  s_inBuf,  kInSize);
    addAsm(kCfgAssembly, s_cfgBuf, sizeof(s_cfgBuf));
    g_config_data_connection_path.class_id    = kCipAssemblyClassCode;
    g_config_data_connection_path.instance_id = kCfgAssembly;
    g_output_assembly_data_connection_path.class_id    = kCipAssemblyClassCode;
    g_output_assembly_data_connection_path.instance_id = kOutAssembly;
    g_input_assembly_data_connection_path.class_id    = kCipAssemblyClassCode;
    g_input_assembly_data_connection_path.instance_id = kInAssembly;
    std::cout << "[EtherNet/IP] OpENer on " << interface << "\n";
    return true;
}

void* EtherNetIPAdapter::stackThreadEntry(void* arg) {
    static_cast<EtherNetIPAdapter*>(arg)->runStack(); return nullptr;
}

void EtherNetIPAdapter::runStack() {
    pthread_setname_np(pthread_self(), "opener-stack");
    if (NetworkHandlerInitialize() != kEipStatusOk) {
        std::cerr << "[EtherNet/IP] NetworkHandlerInitialize failed\n"; return;
    }
    while (running_.load(std::memory_order_acquire)) {
        uint16_t sw  = inStatusWord_.load(std::memory_order_acquire);
        float    asp = inActualSpeed_.load(std::memory_order_acquire);
        int32_t  pos = inPosition_.load(std::memory_order_acquire);
        std::memcpy(&s_inBuf[0], &sw, 2);
        std::memcpy(&s_inBuf[2], &asp, 4);
        std::memcpy(&s_inBuf[6], &pos, 4);
        if (NetworkHandlerProcessCyclic() != kEipStatusOk) break;
    }
    NetworkHandlerFinish();
}

void EtherNetIPAdapter::onOutputDataReceived(const uint8_t* data, uint16_t length) {
    if (length < kOutSize) return;
    uint16_t cw{}; float ts{};
    std::memcpy(&cw, &data[0], 2); std::memcpy(&ts, &data[2], 4);
    outControlWord_.store(cw, std::memory_order_release);
    outTargetSpeed_.store(ts, std::memory_order_release);
}

void EtherNetIPAdapter::onInputDataRequested(uint8_t* data, uint16_t* length) {
    uint16_t sw  = inStatusWord_.load(std::memory_order_acquire);
    float    asp = inActualSpeed_.load(std::memory_order_acquire);
    int32_t  pos = inPosition_.load(std::memory_order_acquire);
    std::memcpy(&data[0], &sw, 2); std::memcpy(&data[2], &asp, 4); std::memcpy(&data[6], &pos, 4);
    *length = kInSize;
}

// ============================================================
#else  // HAVE_OPENER — stub
// ============================================================

bool EtherNetIPAdapter::initialize(const std::string& interface) {
    interface_ = interface;
    std::cout << "[EtherNet/IP] stub mode — OpENer not compiled in\n";
    return true;
}
void* EtherNetIPAdapter::stackThreadEntry(void* /*a*/) { return nullptr; }
void  EtherNetIPAdapter::runStack() {}
void  EtherNetIPAdapter::onOutputDataReceived(const uint8_t*, uint16_t) {}
void  EtherNetIPAdapter::onInputDataRequested(uint8_t*, uint16_t*) {}

#endif  // HAVE_OPENER

// ============================================================
// Common
// ============================================================

void EtherNetIPAdapter::start() {
    running_.store(true, std::memory_order_release);
    pthread_attr_t attr; pthread_attr_init(&attr);
    struct sched_param sp{}; sp.sched_priority = 78;
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    pthread_attr_setschedparam(&attr, &sp);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_create(&stackThread_, &attr, &EtherNetIPAdapter::stackThreadEntry, this);
    pthread_attr_destroy(&attr);
}

void EtherNetIPAdapter::stop() {
    running_.store(false, std::memory_order_release);
    if (stackThread_) { pthread_join(stackThread_, nullptr); stackThread_ = 0; }
#ifdef HAVE_OPENER
    ShutdownCipStack();
#endif
}

uint16_t EtherNetIPAdapter::getControlWord()   const { return outControlWord_.load(std::memory_order_acquire); }
float    EtherNetIPAdapter::getTargetSpeed()   const { return outTargetSpeed_.load(std::memory_order_acquire); }
void     EtherNetIPAdapter::setStatusWord(uint16_t sw)   { inStatusWord_.store(sw, std::memory_order_release); }
void     EtherNetIPAdapter::setActualSpeed(float rpm)    { inActualSpeed_.store(rpm, std::memory_order_release); }
void     EtherNetIPAdapter::setActualPosition(int32_t p) { inPosition_.store(p, std::memory_order_release); }
void     EtherNetIPAdapter::setTorque(float t) { inTorque_.store(static_cast<int16_t>(t * 10.0f), std::memory_order_release); }
