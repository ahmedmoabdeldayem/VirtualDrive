#pragma once
#include "IProtocolAdapter.hpp"
#include <atomic>
#include <cstdint>
#include <pthread.h>
#include <string>

// OpENer EtherNet/IP adapter.
//
// Assembly Object instances:
//   0x64 (output, scanner → adapter): ControlWord UINT16 + TargetSpeed REAL (4B)  = 6 bytes
//   0x65 (input,  adapter → scanner): StatusWord  UINT16 + ActualSpeed REAL +
//                                      ActualPos  DINT   + Torque      INT   = 10 bytes
//   0x66 (config):                     empty (required by EtherNet/IP spec)
//
// The network handler runs in a dedicated thread; the cyclic drive thread
// reads/writes via atomic variables to avoid blocking the RT path.
class EtherNetIPAdapter : public IProtocolAdapter {
public:
    EtherNetIPAdapter();
    ~EtherNetIPAdapter() override;

    bool        initialize(const std::string& interface) override;
    void        start()  override;
    void        stop()   override;

    uint16_t    getControlWord()   const override;
    float       getTargetSpeed()   const override;
    void        setStatusWord(uint16_t sw) override;
    void        setActualSpeed(float rpm) override;
    void        setActualPosition(int32_t pos) override;
    void        setTorque(float torque) override;
    std::string getProtocolName()  const override { return "EtherNet/IP"; }

    static EtherNetIPAdapter* instance();

    // Called by OpENer when the scanner writes Output Assembly 0x64.
    void onOutputDataReceived(const uint8_t* data, uint16_t length);
    // Called by OpENer to populate Input Assembly 0x65 before sending.
    void onInputDataRequested(uint8_t* data, uint16_t* length);

private:
    static void* stackThreadEntry(void* arg);
    void runStack();

    std::string interface_;
    pthread_t   stackThread_   {0};
    std::atomic<bool> running_ {false};

    std::atomic<uint16_t> outControlWord_ {0};
    std::atomic<float>    outTargetSpeed_ {0.0f};

    std::atomic<uint16_t> inStatusWord_   {0};
    std::atomic<float>    inActualSpeed_  {0.0f};
    std::atomic<int32_t>  inPosition_     {0};
    std::atomic<int16_t>  inTorque_       {0};

    static EtherNetIPAdapter* instance_;
};
