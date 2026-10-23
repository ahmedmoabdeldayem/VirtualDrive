#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <pthread.h>
#include <string>
#include "drive/DriveStateMachine.hpp"

struct DriveStatus {
    DriveState  state          {DriveState::NOT_READY};
    float       actualSpeed    {0.0f};
    float       targetSpeed    {0.0f};
    int32_t     position       {0};
    float       torque         {0.0f};
    float       temperature    {25.0f};
    uint16_t    statusWord     {0};
    uint16_t    controlWord    {0};
    uint64_t    cyclicMissCount{0};
    uint64_t    worstJitterUs  {0};
    std::string protocolName;
};

// Minimal HTTP/1.0 server on port 8080 using only POSIX sockets.
// Serves two endpoints:
//   GET /status  -> JSON with current drive telemetry
//   GET /health  -> "OK" or "FAULT: <reason>"
class DriveMonitor {
public:
    explicit DriveMonitor(uint16_t port = 8080);
    ~DriveMonitor();

    DriveMonitor(const DriveMonitor&)            = delete;
    DriveMonitor& operator=(const DriveMonitor&) = delete;

    void start();
    void stop();

    // Called from the cyclic thread each tick; must be wait-free.
    void update(const DriveStatus& status);

private:
    static void* threadEntry(void* arg);
    void serve();
    void handleClient(int client_fd);

    uint16_t          port_;
    int               serverFd_  {-1};
    pthread_t         thread_    {0};
    std::atomic<bool> running_   {false};

    mutable std::mutex mutex_;
    DriveStatus        status_;
};
