#include "drive/MotorModel.hpp"
#include <cmath>
#include <gtest/gtest.h>

static constexpr float kDt    = 0.001f;  // 1 ms
static constexpr float kEps   = 0.01f;   // tolerance for float comparisons

class MotorTest : public ::testing::Test {
protected:
    MotorModel motor;
};

// ---------------------------------------------------------------------------
// Speed ramping
// ---------------------------------------------------------------------------

TEST_F(MotorTest, SpeedRampsTowardTarget) {
    motor.setTargetSpeed(1000.0f);
    motor.update(kDt);
    // Acceleration: 100 RPM/ms × 1 ms = 100 RPM
    EXPECT_NEAR(motor.getActualSpeed(), 100.0f, kEps);
}

TEST_F(MotorTest, SpeedConvergesToTarget) {
    motor.setTargetSpeed(300.0f);
    for (int i = 0; i < 3; ++i) motor.update(kDt);
    EXPECT_NEAR(motor.getActualSpeed(), 300.0f, kEps);
}

TEST_F(MotorTest, SpeedDoesNotOvershoots) {
    motor.setTargetSpeed(50.0f);
    motor.update(kDt);  // delta < 100 RPM → reaches target in one step
    EXPECT_NEAR(motor.getActualSpeed(), 50.0f, kEps);
}

TEST_F(MotorTest, SpeedDeceleratesWhenTargetDrops) {
    motor.setTargetSpeed(500.0f);
    for (int i = 0; i < 5; ++i) motor.update(kDt);
    ASSERT_NEAR(motor.getActualSpeed(), 500.0f, kEps);

    motor.setTargetSpeed(0.0f);
    motor.update(kDt);
    EXPECT_NEAR(motor.getActualSpeed(), 400.0f, kEps);
}

TEST_F(MotorTest, NegativeTargetSpeedRampsDown) {
    motor.setTargetSpeed(-200.0f);
    for (int i = 0; i < 2; ++i) motor.update(kDt);
    EXPECT_NEAR(motor.getActualSpeed(), -200.0f, kEps);
}

// ---------------------------------------------------------------------------
// Position integration
// ---------------------------------------------------------------------------

TEST_F(MotorTest, PositionIntegratesAtKnownSpeed) {
    // At 600 RPM with 1000 counts/rev:
    // counts/s = 600 * 1000 / 60 = 10000 counts/s
    // over 1 ms = 10 counts
    motor.setTargetSpeed(600.0f);
    motor.update(kDt);  // speed reaches 100 RPM first step
    // After 6 ms motor should reach 600 RPM
    for (int i = 0; i < 5; ++i) motor.update(kDt);
    ASSERT_NEAR(motor.getActualSpeed(), 600.0f, kEps);

    const int32_t posBefore = motor.getPosition();
    motor.update(kDt);
    const int32_t posAfter  = motor.getPosition();

    // counts/s at 600 RPM = 10000; 1ms = 10 counts
    EXPECT_EQ(posAfter - posBefore, 10);
}

TEST_F(MotorTest, PositionDoesNotChangeWhenStopped) {
    const int32_t before = motor.getPosition();
    motor.update(kDt);
    EXPECT_EQ(motor.getPosition(), before);
}

TEST_F(MotorTest, PositionAccumulatesOverMultipleTicks) {
    motor.setTargetSpeed(600.0f);
    // Run 6 ticks to reach 600 RPM: tick0=100, tick1=200, ..., tick5=600
    for (int i = 0; i < 6; ++i) motor.update(kDt);
    ASSERT_NEAR(motor.getActualSpeed(), 600.0f, kEps);

    const int32_t posStart = motor.getPosition();
    for (int i = 0; i < 10; ++i) motor.update(kDt);
    const int32_t posEnd = motor.getPosition();

    // 10 ticks at 10 counts/tick = 100 counts
    EXPECT_EQ(posEnd - posStart, 100);
}

// ---------------------------------------------------------------------------
// Temperature model
// ---------------------------------------------------------------------------

TEST_F(MotorTest, TemperatureStartsAtAmbient) {
    EXPECT_NEAR(motor.getTemperature(), 25.0f, kEps);
}

TEST_F(MotorTest, TemperatureRisesWhenRunning) {
    motor.setTargetSpeed(1000.0f);
    for (int i = 0; i < 1000; ++i) motor.update(kDt);
    EXPECT_GT(motor.getTemperature(), 25.0f);
}

TEST_F(MotorTest, TemperatureCoolsWhenStopped) {
    motor.setTargetSpeed(1000.0f);
    for (int i = 0; i < 1000; ++i) motor.update(kDt);
    const float hotTemp = motor.getTemperature();
    ASSERT_GT(hotTemp, 25.0f);

    motor.setTargetSpeed(0.0f);
    for (int i = 0; i < 100; ++i) motor.update(kDt);
    // Speed ramp-down may keep it warm briefly, but we wait enough for cooling
    for (int i = 0; i < 5000; ++i) motor.update(kDt);
    EXPECT_LT(motor.getTemperature(), hotTemp);
}

TEST_F(MotorTest, TemperatureDoesNotGoBelowAmbient) {
    for (int i = 0; i < 100000; ++i) motor.update(kDt);
    EXPECT_GE(motor.getTemperature(), 25.0f - kEps);
}

// ---------------------------------------------------------------------------
// Zero dt edge case
// ---------------------------------------------------------------------------

TEST_F(MotorTest, UpdateWithZeroDtChangesNothing) {
    motor.setTargetSpeed(500.0f);
    motor.update(0.0f);
    EXPECT_NEAR(motor.getActualSpeed(), 0.0f, kEps);
    EXPECT_EQ(motor.getPosition(), 0);
    EXPECT_NEAR(motor.getTemperature(), 25.0f, kEps);
}
