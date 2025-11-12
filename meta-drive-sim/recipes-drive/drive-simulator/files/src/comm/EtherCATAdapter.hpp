#pragma once
#include "IProtocolAdapter.hpp"
#include <atomic>
#include <cstdint>
#include <pthread.h>
#include <string>

// SOES (Simple Open EtherCAT Slave) wraps the ESC (EtherCAT Slave Controller)
// emulation layer. The stack runs in a dedicated SCHED_FIFO thread.
//
// PDO layout (CoE, CiA 402 profile):
//   RxPDO 0x1600 (master → slave):
//     0x6040:00  ControlWord    UINT16
//     0x6042:00  TargetVelocity INT32   (RPM * 100)
//   TxPDO 0x1A00 (slave → master):
//     0x6041:00  StatusWord     UINT16
//     0x606C:00  VelocityActual INT32   (RPM * 100)
//     0x6064:00  PositionActual INT32   (encoder counts)
//     0x6077:00  TorqueActual   INT16   (0.1% of rated)
class EtherCATAdapter : public IProtocolAdapter {
public:
    EtherCATAdapter();
    ~EtherCATAdapter() override;

    bool        initialize(const std::string& interface) override;
    void        start()  override;
    void        stop()   override;

    uint16_t    getControlWord()   const override;
    float       getTargetSpeed()   const override;
    void        setStatusWord(uint16_t sw) override;
    void        setActualSpeed(float rpm) override;
    void        setActualPosition(int32_t pos) override;
    void        setTorque(float torque) override;
    std::string getProtocolName()  const override { return "EtherCAT"; }

    // Called by the SOES application callbacks (C linkage, static dispatch).
    static EtherCATAdapter* instance();
    void onSetOutputs();
    void onGetInputs();
    void onStateChange(uint8_t newState, uint8_t oldState);

private:
    static void* stackThreadEntry(void* arg);
    void runStack();

    std::string interface_;
    pthread_t   stackThread_   {0};
    std::atomic<bool> running_ {false};

    // Process data — written/read by the SOES callbacks (stack thread)
    // and by the cyclic drive thread. Atomic access prevents torn reads.
    std::atomic<uint16_t> rxControlWord_   {0};
    std::atomic<int32_t>  rxTargetVel_     {0};  // RPM*100

    std::atomic<uint16_t> txStatusWord_    {0};
    std::atomic<int32_t>  txActualVel_     {0};  // RPM*100
    std::atomic<int32_t>  txActualPos_     {0};
    std::atomic<int16_t>  txTorque_        {0};  // 0.1%

    static EtherCATAdapter* instance_;
};
