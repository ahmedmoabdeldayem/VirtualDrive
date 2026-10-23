#pragma once
#include "IProtocolAdapter.hpp"
#include <atomic>
#include <cstdint>
#include <pthread.h>
#include <string>

// p-net PROFINET IO-Device adapter.
//
// Module layout (one AR, slot 1, subslot 1):
//   Output data (controller → device, 6 bytes):
//     Offset 0: ControlWord  uint16_t  (CiA 402)
//     Offset 2: TargetSpeed  float     (RPM, IEEE 754)
//
//   Input data (device → controller, 10 bytes):
//     Offset 0: StatusWord   uint16_t  (CiA 402)
//     Offset 2: ActualSpeed  float     (RPM)
//     Offset 6: Position     int32_t   (encoder counts)
//
// Temperature alarm: when motorTemperature_ exceeds 80°C a PROFINET
// process alarm (alarm type 0x0001) is sent via pnet_alarm_send_process_alarm.
class PROFINETAdapter : public IProtocolAdapter {
public:
    PROFINETAdapter();
    ~PROFINETAdapter() override;

    bool        initialize(const std::string& interface) override;
    void        start()  override;
    void        stop()   override;

    uint16_t    getControlWord()   const override;
    float       getTargetSpeed()   const override;
    void        setStatusWord(uint16_t sw) override;
    void        setActualSpeed(float rpm) override;
    void        setActualPosition(int32_t pos) override;
    void        setTorque(float torque) override;
    void        setTemperature(float celsius) override;
    std::string getProtocolName()  const override { return "PROFINET"; }

    // p-net callback targets (called by C-linkage shims below).
    static PROFINETAdapter* instance();

    int onConnect(uint32_t arep);
    int onRelease(uint32_t arep);
    int onWrite(uint32_t arep, uint16_t slot, uint16_t subslot,
                uint16_t idx, uint16_t len, const uint8_t* data);
    int onRead(uint32_t arep, uint16_t slot, uint16_t subslot,
               uint16_t idx, uint8_t** pp_data, uint16_t* p_len);
    void onState(uint32_t arep, int event);

private:
    static void* stackThreadEntry(void* arg);
    void runStack();
    void checkTemperatureAlarm();

    std::string interface_;
    void*       pnet_       {nullptr};  // pnet_t handle (opaque)
    uint32_t    arep_       {0xFFFFFFFF};
    pthread_t   stackThread_{0};
    std::atomic<bool> running_{false};

    // Output process data (controller → device)
    std::atomic<uint16_t> outControlWord_ {0};
    std::atomic<float>    outTargetSpeed_ {0.0f};

    // Input process data (device → controller)
    std::atomic<uint16_t> inStatusWord_   {0};
    std::atomic<float>    inActualSpeed_  {0.0f};
    std::atomic<int32_t>  inPosition_     {0};

    std::atomic<float>    temperature_    {25.0f};
    bool                  alarmPending_   {false};

    static PROFINETAdapter* instance_;
};
