#include "drive/DriveStateMachine.hpp"
#include <gtest/gtest.h>

// CiA 402 control word bit helpers
static constexpr uint16_t CW_SHUTDOWN         = 0x0006;  // EV=1, QS=1, SO=0
static constexpr uint16_t CW_SWITCH_ON        = 0x0007;  // EV=1, QS=1, SO=1
static constexpr uint16_t CW_ENABLE_OP        = 0x000F;  // EV=1, QS=1, SO=1, EO=1
static constexpr uint16_t CW_DISABLE_VOLTAGE  = 0x0000;  // EV=0
static constexpr uint16_t CW_QUICK_STOP       = 0x0002;  // EV=1, QS=0
static constexpr uint16_t CW_FAULT_RESET_SET  = 0x0080;  // FR=1
static constexpr uint16_t CW_FAULT_RESET_CLR  = 0x0000;  // FR=0 (clears edge)

class DSMTest : public ::testing::Test {
protected:
    DriveStateMachine dsm;
};

// ---------------------------------------------------------------------------
// Initial state
// ---------------------------------------------------------------------------

TEST_F(DSMTest, StartsInSwitchOnDisabled) {
    EXPECT_EQ(dsm.getState(), DriveState::SWITCH_ON_DISABLED);
}

TEST_F(DSMTest, InitialStatusWordMatchesCiA402) {
    EXPECT_EQ(dsm.getStatusWord(), 0x0040u);
}

// ---------------------------------------------------------------------------
// Forward path: SWITCH_ON_DISABLED → OPERATION_ENABLED
// ---------------------------------------------------------------------------

TEST_F(DSMTest, ShutdownTransitionsToReadyToSwitchOn) {
    dsm.processControlWord(CW_SHUTDOWN);
    EXPECT_EQ(dsm.getState(), DriveState::READY_TO_SWITCH_ON);
    EXPECT_EQ(dsm.getStatusWord(), 0x0021u);
}

TEST_F(DSMTest, SwitchOnTransitionsToSwitchedOn) {
    dsm.processControlWord(CW_SHUTDOWN);
    dsm.processControlWord(CW_SWITCH_ON);
    EXPECT_EQ(dsm.getState(), DriveState::SWITCHED_ON);
    EXPECT_EQ(dsm.getStatusWord(), 0x0023u);
}

TEST_F(DSMTest, EnableOperationTransitionsToOperationEnabled) {
    dsm.processControlWord(CW_SHUTDOWN);
    dsm.processControlWord(CW_SWITCH_ON);
    dsm.processControlWord(CW_ENABLE_OP);
    EXPECT_EQ(dsm.getState(), DriveState::OPERATION_ENABLED);
    EXPECT_EQ(dsm.getStatusWord(), 0x0037u);
}

// ---------------------------------------------------------------------------
// Reverse paths
// ---------------------------------------------------------------------------

TEST_F(DSMTest, DisableVoltageFromReadyGoesToSwitchOnDisabled) {
    dsm.processControlWord(CW_SHUTDOWN);
    ASSERT_EQ(dsm.getState(), DriveState::READY_TO_SWITCH_ON);
    dsm.processControlWord(CW_DISABLE_VOLTAGE);
    EXPECT_EQ(dsm.getState(), DriveState::SWITCH_ON_DISABLED);
}

TEST_F(DSMTest, ShutdownFromSwitchedOnGoesToReady) {
    dsm.processControlWord(CW_SHUTDOWN);
    dsm.processControlWord(CW_SWITCH_ON);
    ASSERT_EQ(dsm.getState(), DriveState::SWITCHED_ON);
    dsm.processControlWord(CW_SHUTDOWN);
    EXPECT_EQ(dsm.getState(), DriveState::READY_TO_SWITCH_ON);
}

TEST_F(DSMTest, DisableOperationFromOpEnabledGoesToSwitchedOn) {
    dsm.processControlWord(CW_SHUTDOWN);
    dsm.processControlWord(CW_SWITCH_ON);
    dsm.processControlWord(CW_ENABLE_OP);
    ASSERT_EQ(dsm.getState(), DriveState::OPERATION_ENABLED);
    dsm.processControlWord(CW_SWITCH_ON);  // disable operation = Switch On command
    EXPECT_EQ(dsm.getState(), DriveState::SWITCHED_ON);
}

// ---------------------------------------------------------------------------
// Quick stop
// ---------------------------------------------------------------------------

TEST_F(DSMTest, QuickStopFromOperationEnabled) {
    dsm.processControlWord(CW_SHUTDOWN);
    dsm.processControlWord(CW_SWITCH_ON);
    dsm.processControlWord(CW_ENABLE_OP);
    ASSERT_EQ(dsm.getState(), DriveState::OPERATION_ENABLED);
    dsm.processControlWord(CW_QUICK_STOP);
    EXPECT_EQ(dsm.getState(), DriveState::QUICK_STOP_ACTIVE);
    EXPECT_EQ(dsm.getStatusWord(), 0x0007u);
}

TEST_F(DSMTest, DisableVoltageFromQuickStop) {
    dsm.processControlWord(CW_SHUTDOWN);
    dsm.processControlWord(CW_SWITCH_ON);
    dsm.processControlWord(CW_ENABLE_OP);
    dsm.processControlWord(CW_QUICK_STOP);
    dsm.processControlWord(CW_DISABLE_VOLTAGE);
    EXPECT_EQ(dsm.getState(), DriveState::SWITCH_ON_DISABLED);
}

// ---------------------------------------------------------------------------
// Fault injection and reset
// ---------------------------------------------------------------------------

TEST_F(DSMTest, FaultInjectionFromOperationEnabled) {
    dsm.processControlWord(CW_SHUTDOWN);
    dsm.processControlWord(CW_SWITCH_ON);
    dsm.processControlWord(CW_ENABLE_OP);
    dsm.injectFault(FaultCode::OVERCURRENT);
    // Should pass through FAULT_REACTION → FAULT
    EXPECT_EQ(dsm.getActiveFault(), FaultCode::OVERCURRENT);
    // After fault reaction (automatic), state is FAULT
    dsm.processControlWord(CW_ENABLE_OP);  // any CW in FAULT_REACTION → FAULT
    EXPECT_EQ(dsm.getState(), DriveState::FAULT);
    EXPECT_EQ(dsm.getStatusWord(), 0x0008u);
}

TEST_F(DSMTest, FaultResetRisingEdgeClearsToSwitchOnDisabled) {
    dsm.processControlWord(CW_SHUTDOWN);
    dsm.processControlWord(CW_SWITCH_ON);
    dsm.processControlWord(CW_ENABLE_OP);
    dsm.injectFault(FaultCode::OVERTEMPERATURE);
    dsm.processControlWord(CW_ENABLE_OP);  // reach FAULT
    ASSERT_EQ(dsm.getState(), DriveState::FAULT);

    dsm.processControlWord(CW_FAULT_RESET_CLR);  // ensure bit7 was 0
    dsm.processControlWord(CW_FAULT_RESET_SET);  // rising edge
    EXPECT_EQ(dsm.getState(), DriveState::SWITCH_ON_DISABLED);
    EXPECT_EQ(dsm.getActiveFault(), FaultCode::NONE);
}

TEST_F(DSMTest, FaultResetIgnoredIfBit7AlreadySet) {
    dsm.processControlWord(CW_SHUTDOWN);
    dsm.processControlWord(CW_SWITCH_ON);
    dsm.processControlWord(CW_ENABLE_OP);
    dsm.injectFault(FaultCode::ENCODER_ERROR);
    dsm.processControlWord(CW_ENABLE_OP);  // reach FAULT
    ASSERT_EQ(dsm.getState(), DriveState::FAULT);

    dsm.processControlWord(CW_FAULT_RESET_SET);  // bit7=1 (could be rising edge)
    EXPECT_EQ(dsm.getState(), DriveState::SWITCH_ON_DISABLED);  // first time = rising

    dsm.injectFault(FaultCode::ENCODER_ERROR);
    dsm.processControlWord(CW_ENABLE_OP);  // reach FAULT again
    // bit7 is still 1 from last time — not a rising edge
    dsm.processControlWord(CW_FAULT_RESET_SET);
    EXPECT_EQ(dsm.getState(), DriveState::FAULT);  // still in FAULT (no rising edge)
}

// ---------------------------------------------------------------------------
// Invalid transitions are silently ignored
// ---------------------------------------------------------------------------

TEST_F(DSMTest, UnknownCWInSwitchOnDisabledIsIgnored) {
    dsm.processControlWord(0x00FFu);  // no valid command bits
    EXPECT_EQ(dsm.getState(), DriveState::SWITCH_ON_DISABLED);
}

TEST_F(DSMTest, EnableOperationSkipsSwitchedOnNotReachable) {
    dsm.processControlWord(CW_SHUTDOWN);
    // Try to jump to OPERATION_ENABLED from READY_TO_SWITCH_ON
    dsm.processControlWord(CW_ENABLE_OP);
    // CW_ENABLE_OP has bits 7,3,2,1,0 = 0,1,1,1,1 — not a valid command from READY
    // (READY accepts Shutdown (0x06) and Switch On (0x07))
    // 0x0F & 0x8F = 0x0F ≠ 0x06 or 0x07, so no transition
    EXPECT_EQ(dsm.getState(), DriveState::READY_TO_SWITCH_ON);
}

// ---------------------------------------------------------------------------
// Reset
// ---------------------------------------------------------------------------

TEST_F(DSMTest, ResetReturnsToSwitchOnDisabled) {
    dsm.processControlWord(CW_SHUTDOWN);
    dsm.processControlWord(CW_SWITCH_ON);
    dsm.processControlWord(CW_ENABLE_OP);
    ASSERT_EQ(dsm.getState(), DriveState::OPERATION_ENABLED);

    dsm.reset();
    EXPECT_EQ(dsm.getState(), DriveState::SWITCH_ON_DISABLED);
    EXPECT_EQ(dsm.getActiveFault(), FaultCode::NONE);
}
