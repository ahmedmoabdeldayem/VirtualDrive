#pragma once
#include <cstdint>
#include <string>

class IProtocolAdapter {
public:
    virtual ~IProtocolAdapter() = default;

    virtual bool initialize(const std::string& interface) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;

    virtual uint16_t getControlWord() const = 0;
    virtual float getTargetSpeed() const = 0;

    virtual void setStatusWord(uint16_t sw) = 0;
    virtual void setActualSpeed(float rpm) = 0;
    virtual void setActualPosition(int32_t pos) = 0;
    virtual void setTorque(float torque) = 0;

    // Optional: protocol-specific diagnostic channel.
    // Default implementation is a no-op so adapters that lack
    // a native alarm mechanism (EtherCAT, EtherNet/IP) compile unchanged.
    virtual void setTemperature(float /*celsius*/) {}

    virtual std::string getProtocolName() const = 0;
};
