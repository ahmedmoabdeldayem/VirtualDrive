#pragma once
#include <cstdint>

class MotorModel {
public:
    MotorModel() = default;

    // Called every 1 ms; dt_seconds should be 0.001.
    void update(float dt_seconds);

    void  setTargetSpeed(float rpm);
    float getActualSpeed()    const { return actualSpeedRpm_;  }
    int32_t getPosition()     const { return positionCounts_;  }
    float getTorque()         const { return torquePercent_;   }
    float getTemperature()    const { return motorTemperature_;}
    float getDcLinkVoltage()  const { return dcLinkVoltage_;   }

private:
    float   targetSpeedRpm_   {0.0f};
    float   actualSpeedRpm_   {0.0f};
    int32_t positionCounts_   {0};
    float   torquePercent_    {0.0f};
    float   dcLinkVoltage_    {600.0f};
    float   motorTemperature_ {25.0f};

    static constexpr float kAccelRpmPerMs    = 100.0f;
    static constexpr float kCountsPerRev     = 1000.0f;
    static constexpr float kTorqueGain       = 0.05f;
    static constexpr float kThermalRiseRate  = 0.001f;  // °C per tick when running
    static constexpr float kThermalCoolRate  = 0.0005f; // °C per tick when stopped
    static constexpr float kAmbientTemp      = 25.0f;
};
