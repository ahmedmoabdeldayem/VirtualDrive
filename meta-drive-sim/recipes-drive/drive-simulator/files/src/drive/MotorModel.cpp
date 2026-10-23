#include "MotorModel.hpp"
#include <cmath>

void MotorModel::update(float dt_seconds) {
    const float maxDelta = kAccelRpmPerMs * (dt_seconds * 1000.0f);

    const float delta = targetSpeedRpm_ - actualSpeedRpm_;
    if (std::abs(delta) <= maxDelta) {
        actualSpeedRpm_ = targetSpeedRpm_;
    } else if (delta > 0.0f) {
        actualSpeedRpm_ += maxDelta;
    } else {
        actualSpeedRpm_ -= maxDelta;
    }

    // Position integration: RPM → counts/second → counts per dt
    // countsPerSecond = actualSpeedRpm_ * kCountsPerRev / 60.0f
    const float countsPerSecond = actualSpeedRpm_ * kCountsPerRev / 60.0f;
    positionCounts_ += static_cast<int32_t>(countsPerSecond * dt_seconds);

    // Torque proportional to speed error * gain (simplified)
    torquePercent_ = (targetSpeedRpm_ - actualSpeedRpm_) * kTorqueGain;

    // Thermal model: rises when shaft is spinning, cools toward ambient when stopped
    if (std::abs(actualSpeedRpm_) > 1.0f) {
        motorTemperature_ += kThermalRiseRate;
    } else if (motorTemperature_ > kAmbientTemp) {
        motorTemperature_ -= kThermalCoolRate;
        if (motorTemperature_ < kAmbientTemp) {
            motorTemperature_ = kAmbientTemp;
        }
    }
}

void MotorModel::setTargetSpeed(float rpm) {
    targetSpeedRpm_ = rpm;
}
