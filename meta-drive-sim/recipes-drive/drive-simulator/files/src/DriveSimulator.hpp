#pragma once
#include "comm/IProtocolAdapter.hpp"
#include "drive/DriveStateMachine.hpp"
#include "drive/MotorModel.hpp"
#include "monitoring/DriveMonitor.hpp"
#include "rt/CyclicThread.hpp"

class DriveSimulator {
public:
    explicit DriveSimulator(IProtocolAdapter& protocol);
    ~DriveSimulator() = default;

    DriveSimulator(const DriveSimulator&)            = delete;
    DriveSimulator& operator=(const DriveSimulator&) = delete;

    void start();
    void stop();

private:
    void cyclicCallback();

    IProtocolAdapter& protocol_;
    DriveStateMachine stateMachine_;
    MotorModel        motor_;
    DriveMonitor      monitor_;
    CyclicThread      cyclicThread_;
};
