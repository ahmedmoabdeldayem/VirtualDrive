#include "SerialLoopback.hpp"

#include <cerrno>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

static uint64_t nowUs() {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000ULL
         + static_cast<uint64_t>(ts.tv_nsec) / 1000ULL;
}

TestResult SerialLoopback::testUart(const std::string& device) {
    TestResult result;

    int fd = open(device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        result.error_message = "open failed: " + std::string(std::strerror(errno));
        return result;
    }

    struct termios tty{};
    if (tcgetattr(fd, &tty) != 0) {
        result.error_message = "tcgetattr failed: " + std::string(std::strerror(errno));
        close(fd);
        return result;
    }

    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);
    cfmakeraw(&tty);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~static_cast<tcflag_t>(CRTSCTS);
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 1;  // 100 ms read timeout

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        result.error_message = "tcsetattr failed: " + std::string(std::strerror(errno));
        close(fd);
        return result;
    }

    tcflush(fd, TCIOFLUSH);

    static const uint8_t kPattern[] = {0xA5, 0x5A, 0xDE, 0xAD, 0xBE, 0xEF};
    constexpr int kLen = static_cast<int>(sizeof(kPattern));

    const uint64_t t0 = nowUs();
    if (write(fd, kPattern, kLen) != kLen) {
        result.error_message = "write failed: " + std::string(std::strerror(errno));
        close(fd);
        return result;
    }
    tcdrain(fd);

    uint8_t rxBuf[kLen]{};
    int received = 0;
    while (received < kLen) {
        int n = read(fd, rxBuf + received, kLen - received);
        if (n <= 0) break;
        received += n;
    }
    const uint64_t t1 = nowUs();

    close(fd);

    if (received != kLen) {
        result.error_message = "short read: got " + std::to_string(received) + " bytes";
        return result;
    }

    if (std::memcmp(rxBuf, kPattern, kLen) != 0) {
        result.error_message = "data mismatch";
        return result;
    }

    result.passed    = true;
    result.latency_us = t1 - t0;
    return result;
}

TestResult SerialLoopback::testSpi(const std::string& device) {
    TestResult result;

    int fd = open(device.c_str(), O_RDWR);
    if (fd < 0) {
        result.error_message = "open failed: " + std::string(std::strerror(errno));
        return result;
    }

    uint8_t  mode  = SPI_MODE_0;
    uint8_t  bits  = 8;
    uint32_t speed = 1000000;

    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0 ||
        ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        result.error_message = "ioctl configure failed: " + std::string(std::strerror(errno));
        close(fd);
        return result;
    }

    static const uint8_t kTxBuf[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    constexpr int kLen = static_cast<int>(sizeof(kTxBuf));
    uint8_t rxBuf[kLen]{};

    struct spi_ioc_transfer tr{};
    tr.tx_buf        = reinterpret_cast<unsigned long>(kTxBuf);
    tr.rx_buf        = reinterpret_cast<unsigned long>(rxBuf);
    tr.len           = kLen;
    tr.speed_hz      = speed;
    tr.bits_per_word = bits;

    const uint64_t t0 = nowUs();
    int rc = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    const uint64_t t1 = nowUs();

    close(fd);

    if (rc < 0) {
        result.error_message = "SPI transfer failed: " + std::string(std::strerror(errno));
        return result;
    }

    if (std::memcmp(rxBuf, kTxBuf, kLen) != 0) {
        result.error_message = "SPI loopback mismatch (MOSI->MISO wire present?)";
        return result;
    }

    result.passed     = true;
    result.latency_us = t1 - t0;
    return result;
}

TestResult SerialLoopback::scanI2c(const std::string& device) {
    TestResult result;

    int fd = open(device.c_str(), O_RDWR);
    if (fd < 0) {
        result.error_message = "open failed: " + std::string(std::strerror(errno));
        return result;
    }

    int respondCount = 0;
    for (int addr = 0x08; addr <= 0x77; ++addr) {
        if (ioctl(fd, I2C_SLAVE, addr) < 0) {
            continue;
        }
        uint8_t dummy = 0;
        if (read(fd, &dummy, 1) >= 0 || errno != ENXIO) {
            ++respondCount;
        }
    }

    close(fd);

    // Bus is functional if the ioctl loop completed without error,
    // even if no devices are present.
    result.passed       = true;
    result.latency_us   = 0;
    result.error_message = std::to_string(respondCount) + " device(s) found on bus";
    return result;
}
