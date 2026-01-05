#include "DriveSimulator.hpp"

// 1 ms period, SCHED_FIFO priority 80 (below the SOES stack at 80 — adjust if needed).
static constexpr uint32_t kPeriodUs  = 1000;
static constexpr int      kPriority  = 70;
static constexpr float    kDtSeconds = 0.001f;

DriveSimulator::DriveSimulator(IProtocolAdapter& protocol)
    : protocol_(protocol)
    , monitor_(8080)
    , cyclicThread_(kPeriodUs, kPriority, "drive-cyclic",
                    [this]() { cyclicCallback(); })
{}

void DriveSimulator::start() {
    monitor_.start();
    cyclicThread_.start();
}

void DriveSimulator::stop() {
    cyclicThread_.stop();
    monitor_.stop();
}

void DriveSimulator::cyclicCallback() {
    uint16_t cw = protocol_.getControlWord();
    // HTTP /control?cw=N overrides the protocol adapter — used when no
    // fieldbus master is connected (all three adapters return 0 in stub mode).
    if (monitor_.hasHttpCommand()) {
        cw = monitor_.popHttpControlWord();
    }
    stateMachine_.processControlWord(cw);

    const DriveState state = stateMachine_.getState();

    if (state == DriveState::OPERATION_ENABLED) {
        float speed = protocol_.getTargetSpeed();
        if (speed == 0.0f) speed = monitor_.getHttpTargetSpeed();
        motor_.setTargetSpeed(speed);
    } else {
        motor_.setTargetSpeed(0.0f);
    }

    if (state == DriveState::FAULT_REACTION ||
        state == DriveState::FAULT) {
        motor_.setTargetSpeed(0.0f);
    }

    motor_.update(kDtSeconds);

    if (motor_.getTemperature() > 80.0f &&
        state != DriveState::FAULT &&
        state != DriveState::FAULT_REACTION) {
        stateMachine_.injectFault(FaultCode::OVERTEMPERATURE);
    }

    const uint16_t sw = stateMachine_.getStatusWord();
    protocol_.setStatusWord(sw);
    protocol_.setActualSpeed(motor_.getActualSpeed());
    protocol_.setActualPosition(motor_.getPosition());
    protocol_.setTorque(motor_.getTorque());
    protocol_.setTemperature(motor_.getTemperature());

    DriveStatus status;
    status.state          = state;
    status.actualSpeed    = motor_.getActualSpeed();
    status.targetSpeed    = protocol_.getTargetSpeed();
    status.position       = motor_.getPosition();
    status.torque         = motor_.getTorque();
    status.temperature    = motor_.getTemperature();
    status.statusWord     = sw;
    status.controlWord    = cw;
    status.cyclicMissCount= cyclicThread_.getMissCount();
    status.worstJitterUs  = cyclicThread_.getWorstJitterUs();
    status.protocolName   = protocol_.getProtocolName();
    monitor_.update(status);
}
