#include "DriveMonitor.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

static const char* stateToString(DriveState s) {
    switch (s) {
        case DriveState::NOT_READY:          return "NOT_READY";
        case DriveState::SWITCH_ON_DISABLED: return "SWITCH_ON_DISABLED";
        case DriveState::READY_TO_SWITCH_ON: return "READY_TO_SWITCH_ON";
        case DriveState::SWITCHED_ON:        return "SWITCHED_ON";
        case DriveState::OPERATION_ENABLED:  return "OPERATION_ENABLED";
        case DriveState::QUICK_STOP_ACTIVE:  return "QUICK_STOP_ACTIVE";
        case DriveState::FAULT_REACTION:     return "FAULT_REACTION";
        case DriveState::FAULT:              return "FAULT";
    }
    return "UNKNOWN";
}

DriveMonitor::DriveMonitor(uint16_t port)
    : port_(port)
{}

DriveMonitor::~DriveMonitor() {
    stop();
}

void DriveMonitor::start() {
    serverFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd_ < 0) {
        std::cerr << "[Monitor] socket failed: " << std::strerror(errno) << "\n";
        return;
    }

    int opt = 1;
    setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port_);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverFd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "[Monitor] bind failed: " << std::strerror(errno) << "\n";
        close(serverFd_);
        serverFd_ = -1;
        return;
    }

    if (listen(serverFd_, 4) < 0) {
        std::cerr << "[Monitor] listen failed: " << std::strerror(errno) << "\n";
        close(serverFd_);
        serverFd_ = -1;
        return;
    }

    running_.store(true, std::memory_order_release);
    pthread_create(&thread_, nullptr, &DriveMonitor::threadEntry, this);
    std::cout << "[Monitor] HTTP server on port " << port_ << "\n";
}

void DriveMonitor::stop() {
    running_.store(false, std::memory_order_release);
    if (serverFd_ >= 0) {
        shutdown(serverFd_, SHUT_RDWR);
        close(serverFd_);
        serverFd_ = -1;
    }
    if (thread_) {
        pthread_join(thread_, nullptr);
        thread_ = 0;
    }
}

void DriveMonitor::update(const DriveStatus& status) {
    std::lock_guard<std::mutex> lock(mutex_);
    status_ = status;
}

void* DriveMonitor::threadEntry(void* arg) {
    static_cast<DriveMonitor*>(arg)->serve();
    return nullptr;
}

void DriveMonitor::serve() {
    pthread_setname_np(pthread_self(), "drive-monitor");

    while (running_.load(std::memory_order_acquire)) {
        int client = accept(serverFd_, nullptr, nullptr);
        if (client < 0) {
            if (running_.load(std::memory_order_acquire)) {
                std::cerr << "[Monitor] accept error: " << std::strerror(errno) << "\n";
            }
            break;
        }
        handleClient(client);
        close(client);
    }
}

void DriveMonitor::handleClient(int client_fd) {
    char req[512]{};
    int n = recv(client_fd, req, sizeof(req) - 1, 0);
    if (n <= 0) return;
    req[n] = '\0';

    DriveStatus snap;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snap = status_;
    }

    std::string body;
    std::string contentType;
    int statusCode = 200;

    if (std::strncmp(req, "GET /status ", 12) == 0) {
        std::ostringstream j;
        j << "{\n"
          << "  \"state\": \""        << stateToString(snap.state) << "\",\n"
          << "  \"actualSpeed\": "    << snap.actualSpeed       << ",\n"
          << "  \"targetSpeed\": "    << snap.targetSpeed       << ",\n"
          << "  \"position\": "       << snap.position          << ",\n"
          << "  \"torque\": "         << snap.torque            << ",\n"
          << "  \"temperature\": "    << snap.temperature       << ",\n"
          << "  \"statusWord\": "     << snap.statusWord        << ",\n"
          << "  \"controlWord\": "    << snap.controlWord       << ",\n"
          << "  \"cyclicMissCount\": "<< snap.cyclicMissCount   << ",\n"
          << "  \"worstJitterUs\": "  << snap.worstJitterUs     << ",\n"
          << "  \"protocolName\": \"" << snap.protocolName      << "\"\n"
          << "}\n";
        body        = j.str();
        contentType = "application/json";

    } else if (std::strncmp(req, "GET /health ", 12) == 0) {
        if (snap.state == DriveState::FAULT || snap.state == DriveState::FAULT_REACTION) {
            body = "FAULT: " + std::string(stateToString(snap.state)) + "\n";
        } else {
            body = "OK\n";
        }
        contentType = "text/plain";

    } else if (std::strncmp(req, "GET /control", 12) == 0) {
        const char* cwParam = std::strstr(req, "cw=");
        if (cwParam) {
            uint16_t cw = static_cast<uint16_t>(std::strtol(cwParam + 3, nullptr, 10));
            httpCw_.store(cw, std::memory_order_release);
            httpCwPending_.store(true, std::memory_order_release);
        }
        const char* spParam = std::strstr(req, "speed=");
        if (spParam) {
            float sp = std::strtof(spParam + 6, nullptr);
            httpSpeedX10_.store(static_cast<uint32_t>(sp * 10.0f), std::memory_order_release);
        }
        body        = "{\"ok\":true}\n";
        contentType = "application/json";

    } else {
        body        = "404 Not Found\n";
        contentType = "text/plain";
        statusCode  = 404;
    }

    std::ostringstream resp;
    resp << "HTTP/1.0 " << statusCode
         << (statusCode == 200 ? " OK" : " Not Found") << "\r\n"
         << "Content-Type: " << contentType << "\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "Connection: close\r\n"
         << "\r\n"
         << body;

    std::string respStr = resp.str();
    send(client_fd, respStr.c_str(), respStr.size(), MSG_NOSIGNAL);
}

bool DriveMonitor::hasHttpCommand() const {
    return httpCwPending_.load(std::memory_order_acquire);
}

uint16_t DriveMonitor::popHttpControlWord() {
    httpCwPending_.store(false, std::memory_order_release);
    return httpCw_.load(std::memory_order_acquire);
}

float DriveMonitor::getHttpTargetSpeed() const {
    return static_cast<float>(httpSpeedX10_.load(std::memory_order_acquire)) / 10.0f;
}
