#pragma once
#include <cstdint>
#include <string>

struct TestResult {
    bool        passed       {false};
    uint64_t    latency_us   {0};
    std::string error_message;
};

class SerialLoopback {
public:
    // UART loopback: /dev/ttyAMA0 at 115200 8N1.
    // Requires TX physically shorted to RX (loopback wire).
    static TestResult testUart(const std::string& device = "/dev/ttyAMA0");

    // SPI loopback: /dev/spidev0.0 at 1 MHz mode 0.
    // Requires MOSI physically shorted to MISO.
    static TestResult testSpi(const std::string& device = "/dev/spidev0.0");

    // I2C bus scan: probes addresses 0x08–0x77 for any ACK.
    // Returns passed=true when the bus itself responds (even with no devices).
    static TestResult scanI2c(const std::string& device = "/dev/i2c-1");
};
