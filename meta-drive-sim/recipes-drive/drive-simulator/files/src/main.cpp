#include "DriveSimulator.hpp"
#include "comm/EtherCATAdapter.hpp"
#include "comm/EtherNetIPAdapter.hpp"
#include "comm/IProtocolAdapter.hpp"
#include "comm/PROFINETAdapter.hpp"

#include <csignal>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <time.h>

static volatile sig_atomic_t g_shutdown = 0;

static void handleSignal(int /*sig*/) {
    g_shutdown = 1;
}

static void printUsage(const char* argv0) {
    std::cerr << "Usage: " << argv0
              << " [--protocol ethercat|profinet|ethernetip]"
              << " [--interface <ifname>]\n";
}

int main(int argc, char* argv[]) {
    std::string protocol  = "ethercat";
    std::string interface = "eth0";

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--protocol") == 0 && i + 1 < argc) {
            protocol = argv[++i];
        } else if (std::strcmp(argv[i], "--interface") == 0 && i + 1 < argc) {
            interface = argv[++i];
        } else if (std::strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        }
    }

    std::unique_ptr<IProtocolAdapter> adapter;
    if (protocol == "ethercat") {
        adapter = std::make_unique<EtherCATAdapter>();
    } else if (protocol == "profinet") {
        adapter = std::make_unique<PROFINETAdapter>();
    } else if (protocol == "ethernetip") {
        adapter = std::make_unique<EtherNetIPAdapter>();
    } else {
        std::cerr << "Unknown protocol: " << protocol << "\n";
        printUsage(argv[0]);
        return 1;
    }

    std::cout << "[main] Starting VirtualDrive with "
              << adapter->getProtocolName()
              << " on " << interface << "\n";

    if (!adapter->initialize(interface)) {
        std::cerr << "[main] Failed to initialize " << adapter->getProtocolName() << "\n";
        return 2;
    }

    struct sigaction sa{};
    sa.sa_handler = handleSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT,  &sa, nullptr);

    DriveSimulator simulator(*adapter);

    adapter->start();
    simulator.start();

    std::cout << "[main] Running — send SIGTERM or SIGINT to stop\n";

    while (!g_shutdown) {
        struct timespec ts{1, 0};
        nanosleep(&ts, nullptr);
    }

    std::cout << "[main] Shutdown signal received\n";
    simulator.stop();
    adapter->stop();

    return 0;
}
