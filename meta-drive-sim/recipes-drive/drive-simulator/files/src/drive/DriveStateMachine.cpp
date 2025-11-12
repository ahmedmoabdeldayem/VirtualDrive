#include "DriveStateMachine.hpp"
#include <iostream>

// CiA 402 status word encodings (Table 29 of the standard)
static constexpr uint16_t SW_NOT_READY          = 0x0000;
static constexpr uint16_t SW_SWITCH_ON_DISABLED = 0x0040;
static constexpr uint16_t SW_READY_TO_SWITCH_ON = 0x0021;
static constexpr uint16_t SW_SWITCHED_ON        = 0x0023;
static constexpr uint16_t SW_OPERATION_ENABLED  = 0x0037;
static constexpr uint16_t SW_QUICK_STOP_ACTIVE  = 0x0007;
static constexpr uint16_t SW_FAULT_REACTION     = 0x000F;
static constexpr uint16_t SW_FAULT              = 0x0008;

// CiA 402 control word command masks (Table 30)
// "Shutdown":         bits [7,3,2,1,0] = 0,0,1,1,0  → mask 0x8F == 0x06
// "Switch On":        bits [7,3,2,1,0] = 0,0,1,1,1  → mask 0x8F == 0x07
// "Enable Operation": bits [7,3,2,1,0] = 0,1,1,1,1  → mask 0x8F == 0x0F
// "Disable Voltage":  bit  [1] = 0                  → mask 0x02 == 0x00
// "Quick Stop":       bits [2,1] = 0,1               → mask 0x06 == 0x02
// "Fault Reset":      rising edge on bit 7

class State {
public:
    virtual ~State() = default;
    virtual DriveState getState()    const = 0;
    virtual uint16_t   getStatusWord() const = 0;
    virtual State*     processControlWord(uint16_t cw, DriveStateMachine& sm) = 0;
    virtual State*     onFault(FaultCode code, DriveStateMachine& sm) = 0;
};

// ---------------------------------------------------------------------------
// Concrete states
// ---------------------------------------------------------------------------

class NotReadyState final : public State {
public:
    DriveState getState()     const override { return DriveState::NOT_READY; }
    uint16_t   getStatusWord() const override { return SW_NOT_READY; }
    State* processControlWord(uint16_t /*cw*/, DriveStateMachine& sm) override {
        // Automatic transition to SWITCH_ON_DISABLED after power-on init
        return sm.stateSwitchOnDisabled();
    }
    State* onFault(FaultCode /*code*/, DriveStateMachine& sm) override {
        return sm.stateFaultReaction();
    }
};

class SwitchOnDisabledState final : public State {
public:
    DriveState getState()     const override { return DriveState::SWITCH_ON_DISABLED; }
    uint16_t   getStatusWord() const override { return SW_SWITCH_ON_DISABLED; }
    State* processControlWord(uint16_t cw, DriveStateMachine& sm) override {
        // Shutdown command: EV=1, QS=1, SO=0 (bits 7,3,2,1,0) = 0,0,1,1,0
        if ((cw & 0x8Fu) == 0x06u) {
            return sm.stateReadyToSwitchOn();
        }
        return this;
    }
    State* onFault(FaultCode /*code*/, DriveStateMachine& sm) override {
        return sm.stateFaultReaction();
    }
};

class ReadyToSwitchOnState final : public State {
public:
    DriveState getState()     const override { return DriveState::READY_TO_SWITCH_ON; }
    uint16_t   getStatusWord() const override { return SW_READY_TO_SWITCH_ON; }
    State* processControlWord(uint16_t cw, DriveStateMachine& sm) override {
        // Disable Voltage: bit 1 = 0
        if ((cw & 0x02u) == 0x00u) {
            return sm.stateSwitchOnDisabled();
        }
        // Switch On: 0,0,1,1,1
        if ((cw & 0x8Fu) == 0x07u) {
            return sm.stateSwitchedOn();
        }
        return this;
    }
    State* onFault(FaultCode /*code*/, DriveStateMachine& sm) override {
        return sm.stateFaultReaction();
    }
};

class SwitchedOnState final : public State {
public:
    DriveState getState()     const override { return DriveState::SWITCHED_ON; }
    uint16_t   getStatusWord() const override { return SW_SWITCHED_ON; }
    State* processControlWord(uint16_t cw, DriveStateMachine& sm) override {
        if ((cw & 0x02u) == 0x00u) {
            return sm.stateSwitchOnDisabled();
        }
        if ((cw & 0x8Fu) == 0x06u) {
            return sm.stateReadyToSwitchOn();
        }
        // Enable Operation: 0,1,1,1,1
        if ((cw & 0x8Fu) == 0x0Fu) {
            return sm.stateOperationEnabled();
        }
        return this;
    }
    State* onFault(FaultCode /*code*/, DriveStateMachine& sm) override {
        return sm.stateFaultReaction();
    }
};

class OperationEnabledState final : public State {
public:
    DriveState getState()     const override { return DriveState::OPERATION_ENABLED; }
    uint16_t   getStatusWord() const override { return SW_OPERATION_ENABLED; }
    State* processControlWord(uint16_t cw, DriveStateMachine& sm) override {
        if ((cw & 0x02u) == 0x00u) {
            return sm.stateSwitchOnDisabled();
        }
        if ((cw & 0x8Fu) == 0x06u) {
            return sm.stateReadyToSwitchOn();
        }
        // Quick Stop: bit2=0, bit1=1 → mask 0x06 == 0x02
        if ((cw & 0x06u) == 0x02u) {
            return sm.stateQuickStopActive();
        }
        // Disable Operation (same bit pattern as Switch On command): 0,0,1,1,1
        if ((cw & 0x8Fu) == 0x07u) {
            return sm.stateSwitchedOn();
        }
        return this;
    }
    State* onFault(FaultCode /*code*/, DriveStateMachine& sm) override {
        return sm.stateFaultReaction();
    }
};

class QuickStopActiveState final : public State {
public:
    DriveState getState()     const override { return DriveState::QUICK_STOP_ACTIVE; }
    uint16_t   getStatusWord() const override { return SW_QUICK_STOP_ACTIVE; }
    State* processControlWord(uint16_t cw, DriveStateMachine& sm) override {
        if ((cw & 0x02u) == 0x00u) {
            return sm.stateSwitchOnDisabled();
        }
        // Enable Operation resumes (optional per CiA 402)
        if ((cw & 0x8Fu) == 0x0Fu) {
            return sm.stateOperationEnabled();
        }
        return this;
    }
    State* onFault(FaultCode /*code*/, DriveStateMachine& sm) override {
        return sm.stateFaultReaction();
    }
};

class FaultReactionState final : public State {
public:
    DriveState getState()     const override { return DriveState::FAULT_REACTION; }
    uint16_t   getStatusWord() const override { return SW_FAULT_REACTION; }
    State* processControlWord(uint16_t /*cw*/, DriveStateMachine& sm) override {
        // Automatic transition to FAULT after reaction completes (immediate here)
        return sm.stateFault();
    }
    State* onFault(FaultCode /*code*/, DriveStateMachine& /*sm*/) override {
        return this;
    }
};

class FaultState final : public State {
public:
    DriveState getState()     const override { return DriveState::FAULT; }
    uint16_t   getStatusWord() const override { return SW_FAULT; }
    State* processControlWord(uint16_t /*cw*/, DriveStateMachine& sm) override {
        // Fault Reset is detected as a rising edge in DriveStateMachine::processControlWord
        // and redirects to SwitchOnDisabled before calling here. This path
        // handles non-reset control words while in FAULT.
        return sm.stateSwitchOnDisabled();
    }
    State* onFault(FaultCode /*code*/, DriveStateMachine& /*sm*/) override {
        return this;
    }
};

// ---------------------------------------------------------------------------
// StateStorage: owns all concrete state instances on the stack (no heap)
// ---------------------------------------------------------------------------

struct DriveStateMachine::StateStorage {
    NotReadyState          notReady;
    SwitchOnDisabledState  switchOnDisabled;
    ReadyToSwitchOnState   readyToSwitchOn;
    SwitchedOnState        switchedOn;
    OperationEnabledState  operationEnabled;
    QuickStopActiveState   quickStopActive;
    FaultReactionState     faultReaction;
    FaultState             fault;
};

// ---------------------------------------------------------------------------
// DriveStateMachine
// ---------------------------------------------------------------------------

DriveStateMachine::DriveStateMachine()
    : storage_(std::make_unique<StateStorage>())
    , current_(&storage_->notReady)
    , activeFault_(FaultCode::NONE)
    , prevFaultResetBit_(0)
{
    // Immediately advance from NOT_READY to SWITCH_ON_DISABLED (power-on)
    current_ = &storage_->switchOnDisabled;
    std::cout << "[DSM] SWITCH_ON_DISABLED\n";
}

DriveStateMachine::~DriveStateMachine() = default;

void DriveStateMachine::processControlWord(uint16_t cw) {
    const uint8_t faultResetBit = static_cast<uint8_t>((cw >> 7) & 0x01u);
    const bool faultResetRisingEdge = (faultResetBit == 1) && (prevFaultResetBit_ == 0);
    prevFaultResetBit_ = faultResetBit;

    State* next = nullptr;
    if (faultResetRisingEdge && current_->getState() == DriveState::FAULT) {
        activeFault_ = FaultCode::NONE;
        next = &storage_->switchOnDisabled;
    } else {
        next = current_->processControlWord(cw, *this);
    }

    if (next != current_) {
        std::cout << "[DSM] -> " << static_cast<int>(next->getState()) << "\n";
        current_ = next;
    }
}

uint16_t DriveStateMachine::getStatusWord() const {
    uint16_t sw = current_->getStatusWord();
    if (activeFault_ != FaultCode::NONE) {
        sw |= 0x0080u;  // Warning bit while fault code is set
    }
    return sw;
}

DriveState DriveStateMachine::getState() const {
    return current_->getState();
}

FaultCode DriveStateMachine::getActiveFault() const {
    return activeFault_;
}

void DriveStateMachine::injectFault(FaultCode code) {
    activeFault_ = code;
    State* next = current_->onFault(code, *this);
    if (next != current_) {
        std::cout << "[DSM] FAULT injected: 0x"
                  << std::hex << static_cast<uint16_t>(code) << std::dec << "\n";
        current_ = next;
    }
}

void DriveStateMachine::reset() {
    activeFault_       = FaultCode::NONE;
    prevFaultResetBit_ = 0;
    current_ = &storage_->switchOnDisabled;
}

State* DriveStateMachine::stateNotReady()          { return &storage_->notReady; }
State* DriveStateMachine::stateSwitchOnDisabled()  { return &storage_->switchOnDisabled; }
State* DriveStateMachine::stateReadyToSwitchOn()   { return &storage_->readyToSwitchOn; }
State* DriveStateMachine::stateSwitchedOn()        { return &storage_->switchedOn; }
State* DriveStateMachine::stateOperationEnabled()  { return &storage_->operationEnabled; }
State* DriveStateMachine::stateQuickStopActive()   { return &storage_->quickStopActive; }
State* DriveStateMachine::stateFaultReaction()     { return &storage_->faultReaction; }
State* DriveStateMachine::stateFault()             { return &storage_->fault; }
