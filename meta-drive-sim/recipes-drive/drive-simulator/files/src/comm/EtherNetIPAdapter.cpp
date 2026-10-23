#include "EtherNetIPAdapter.hpp"

#include <cstring>
#include <iostream>
#include <pthread.h>

// OpENer headers — paths resolved by CMake include directories
extern "C" {
#include "opener_api.h"
#include "cipcommon.h"
#include "cipassembly.h"
#include "networkhandler.h"
}

EtherNetIPAdapter* EtherNetIPAdapter::instance_ = nullptr;

// Assembly instance numbers (EDS / ODVA spec)
static constexpr EipUint32 kOutputAssembly = 0x64;  // scanner → adapter
static constexpr EipUint32 kInputAssembly  = 0x65;  // adapter → scanner
static constexpr EipUint32 kConfigAssembly = 0x66;  // config (empty)

static constexpr EipUint16 kOutputSize = 6;   // ControlWord(2) + TargetSpeed(4)
static constexpr EipUint16 kInputSize  = 10;  // StatusWord(2) + ActualSpeed(4) + ActualPos(4)
// Note: Torque (INT16, 2 bytes) omitted from input assembly to meet 10-byte spec size.
// Extend to 12 bytes and update ODVA if torque transmission is needed.

static uint8_t s_outputBuf[kOutputSize]{};
static uint8_t s_inputBuf[kInputSize]{};
static uint8_t s_configBuf[4]{};  // Minimum non-empty config assembly

// ---------------------------------------------------------------------------
// OpENer connection callbacks — called when scanner opens/closes connections
// ---------------------------------------------------------------------------

extern "C" {

static EipStatus onConnectionOpened(CipConnectionObject* /*conn*/) {
    std::cout << "[EtherNet/IP] Connection opened\n";
    return kEipStatusOk;
}

static void onConnectionClosed(CipConnectionObject* /*conn*/) {
    std::cout << "[EtherNet/IP] Connection closed\n";
}

// Called by OpENer after output data is received from scanner (T→O direction)
static void onOutputDataReceived(void* /*data*/) {
    if (EtherNetIPAdapter::instance()) {
        EtherNetIPAdapter::instance()->onOutputDataReceived(
            s_outputBuf, kOutputSize);
    }
}

}  // extern "C"

// ---------------------------------------------------------------------------
// EtherNetIPAdapter
// ---------------------------------------------------------------------------

EtherNetIPAdapter::EtherNetIPAdapter() {
    instance_ = this;
}

EtherNetIPAdapter::~EtherNetIPAdapter() {
    stop();
    instance_ = nullptr;
}

EtherNetIPAdapter* EtherNetIPAdapter::instance() {
    return instance_;
}

bool EtherNetIPAdapter::initialize(const std::string& interface) {
    interface_ = interface;

    // Initialize CIP stack
    CipStackInit(0xC0A80064);  // placeholder IP; replaced by network at runtime

    // Output Assembly (scanner → adapter): receives ControlWord + TargetSpeed
    CipClass* assembly_class = GetCipClass(kCipAssemblyClassCode);
    CipInstance* output_inst = AddCipInstance(assembly_class, kOutputAssembly);
    InsertAttribute(output_inst, 3, kCipByteArray, EncodeCipByteArray,
                    nullptr, kGetableSingleAndAll | kSetable);
    SetAssemblyInstanceData(output_inst, s_outputBuf, kOutputSize);

    // Input Assembly (adapter → scanner): sends StatusWord + ActualSpeed + ActualPos
    CipInstance* input_inst  = AddCipInstance(assembly_class, kInputAssembly);
    InsertAttribute(input_inst, 3, kCipByteArray, EncodeCipByteArray,
                    nullptr, kGetableSingleAndAll | kSetable);
    SetAssemblyInstanceData(input_inst, s_inputBuf, kInputSize);

    // Config Assembly (required by EtherNet/IP adapter specification)
    CipInstance* config_inst = AddCipInstance(assembly_class, kConfigAssembly);
    InsertAttribute(config_inst, 3, kCipByteArray, EncodeCipByteArray,
                    nullptr, kGetableSingleAndAll | kSetable);
    SetAssemblyInstanceData(config_inst, s_configBuf, sizeof(s_configBuf));

    // Register connection configuration (Class 1 cyclic connection)
    g_config_data_connection_path.class_id    = kCipAssemblyClassCode;
    g_config_data_connection_path.instance_id = kConfigAssembly;
    g_output_assembly_data_connection_path.class_id    = kCipAssemblyClassCode;
    g_output_assembly_data_connection_path.instance_id = kOutputAssembly;
    g_input_assembly_data_connection_path.class_id    = kCipAssemblyClassCode;
    g_input_assembly_data_connection_path.instance_id = kInputAssembly;

    std::cout << "[EtherNet/IP] Initialized on " << interface << "\n";
    return true;
}

void EtherNetIPAdapter::start() {
    running_.store(true, std::memory_order_release);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    struct sched_param sp{};
    sp.sched_priority = 78;
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    pthread_attr_setschedparam(&attr, &sp);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_create(&stackThread_, &attr, &EtherNetIPAdapter::stackThreadEntry, this);
    pthread_attr_destroy(&attr);
}

void EtherNetIPAdapter::stop() {
    running_.store(false, std::memory_order_release);
    if (stackThread_) {
        pthread_join(stackThread_, nullptr);
        stackThread_ = 0;
    }
    ShutdownCipStack();
}

void* EtherNetIPAdapter::stackThreadEntry(void* arg) {
    static_cast<EtherNetIPAdapter*>(arg)->runStack();
    return nullptr;
}

void EtherNetIPAdapter::runStack() {
    pthread_setname_np(pthread_self(), "opener-stack");

    if (NetworkHandlerInitialize() != kEipStatusOk) {
        std::cerr << "[EtherNet/IP] NetworkHandlerInitialize failed\n";
        return;
    }

    while (running_.load(std::memory_order_acquire)) {
        // Update input assembly buffer before network handler processes it
        uint16_t sw  = inStatusWord_.load(std::memory_order_acquire);
        float    asp = inActualSpeed_.load(std::memory_order_acquire);
        int32_t  pos = inPosition_.load(std::memory_order_acquire);
        std::memcpy(&s_inputBuf[0], &sw,  2);
        std::memcpy(&s_inputBuf[2], &asp, 4);
        std::memcpy(&s_inputBuf[6], &pos, 4);

        EipStatus status = NetworkHandlerProcessCyclic();
        if (status != kEipStatusOk) {
            break;
        }
    }

    NetworkHandlerFinish();
}

void EtherNetIPAdapter::onOutputDataReceived(const uint8_t* data, uint16_t length) {
    if (length < kOutputSize) return;

    uint16_t cw{};
    float    ts{};
    std::memcpy(&cw, &data[0], 2);
    std::memcpy(&ts, &data[2], 4);
    outControlWord_.store(cw, std::memory_order_release);
    outTargetSpeed_.store(ts, std::memory_order_release);
}

void EtherNetIPAdapter::onInputDataRequested(uint8_t* data, uint16_t* length) {
    uint16_t sw  = inStatusWord_.load(std::memory_order_acquire);
    float    asp = inActualSpeed_.load(std::memory_order_acquire);
    int32_t  pos = inPosition_.load(std::memory_order_acquire);
    std::memcpy(&data[0], &sw,  2);
    std::memcpy(&data[2], &asp, 4);
    std::memcpy(&data[6], &pos, 4);
    *length = kInputSize;
}

uint16_t EtherNetIPAdapter::getControlWord()   const { return outControlWord_.load(std::memory_order_acquire); }
float    EtherNetIPAdapter::getTargetSpeed()   const { return outTargetSpeed_.load(std::memory_order_acquire); }
void     EtherNetIPAdapter::setStatusWord(uint16_t sw)   { inStatusWord_.store(sw, std::memory_order_release); }
void     EtherNetIPAdapter::setActualSpeed(float rpm)    { inActualSpeed_.store(rpm, std::memory_order_release); }
void     EtherNetIPAdapter::setActualPosition(int32_t p) { inPosition_.store(p, std::memory_order_release); }
void     EtherNetIPAdapter::setTorque(float torque)      {
    inTorque_.store(static_cast<int16_t>(torque * 10.0f), std::memory_order_release);
}
