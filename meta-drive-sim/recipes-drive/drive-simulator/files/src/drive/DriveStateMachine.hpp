#pragma once
#include <cstdint>
#include <memory>

enum class DriveState : uint8_t {
    NOT_READY          = 0,
    SWITCH_ON_DISABLED = 1,
    READY_TO_SWITCH_ON = 2,
    SWITCHED_ON        = 3,
    OPERATION_ENABLED  = 4,
    QUICK_STOP_ACTIVE  = 5,
    FAULT_REACTION     = 6,
    FAULT              = 7
};

enum class FaultCode : uint16_t {
    NONE                 = 0x0000,
    OVERCURRENT          = 0x2310,
    OVERVOLTAGE          = 0x3210,
    OVERTEMPERATURE      = 0x4210,
    ENCODER_ERROR        = 0x7310,
    COMMUNICATION_ERROR  = 0x8110
};

// Forward-declared; concrete state classes live entirely in the .cpp.
class State;

class DriveStateMachine {
public:
    DriveStateMachine();
    ~DriveStateMachine();

    // Called every cyclic tick with the master's control word.
    void processControlWord(uint16_t cw);

    uint16_t getStatusWord() const;
    DriveState getState() const;
    FaultCode  getActiveFault() const;

    void injectFault(FaultCode code);
    void reset();

    // Accessors used by concrete State subclasses during transitions.
    // They return raw pointers into pre-allocated storage — no heap involved.
    State* stateNotReady();
    State* stateSwitchOnDisabled();
    State* stateReadyToSwitchOn();
    State* stateSwitchedOn();
    State* stateOperationEnabled();
    State* stateQuickStopActive();
    State* stateFaultReaction();
    State* stateFault();

private:
    struct StateStorage;
    std::unique_ptr<StateStorage> storage_;

    State*     current_;
    FaultCode  activeFault_;
    uint8_t    prevFaultResetBit_;  // edge detection for bit 7 (Fault Reset)
};
