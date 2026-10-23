#include "CyclicThread.hpp"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/mman.h>
#include <time.h>

CyclicThread::CyclicThread(uint32_t period_us,
                           int      priority,
                           std::string name,
                           std::function<void()> callback)
    : periodUs_(period_us)
    , priority_(priority)
    , name_(std::move(name))
    , callback_(std::move(callback))
{}

CyclicThread::~CyclicThread() {
    stop();
}

void CyclicThread::start() {
    running_.store(true, std::memory_order_release);
    int rc = pthread_create(&thread_, nullptr, &CyclicThread::threadEntry, this);
    if (rc != 0) {
        std::cerr << "[CyclicThread] pthread_create failed: " << std::strerror(rc) << "\n";
        running_.store(false, std::memory_order_release);
    }
}

void CyclicThread::stop() {
    running_.store(false, std::memory_order_release);
    if (thread_) {
        pthread_join(thread_, nullptr);
        thread_ = 0;
    }
}

void* CyclicThread::threadEntry(void* arg) {
    static_cast<CyclicThread*>(arg)->run();
    return nullptr;
}

void CyclicThread::run() {
    // Lock all current and future memory pages — prevents page-fault latency
    // in the RT loop. Requires CAP_IPC_LOCK or RLIMIT_MEMLOCK=unlimited.
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        std::cerr << "[CyclicThread] mlockall failed: " << std::strerror(errno) << "\n";
    }

    // Elevate to SCHED_FIFO
    struct sched_param sp{};
    sp.sched_priority = priority_;
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) != 0) {
        std::cerr << "[CyclicThread] setschedparam failed: " << std::strerror(errno) << "\n";
    }

    // Set thread name (visible in /proc and htop)
    pthread_setname_np(pthread_self(), name_.c_str());

    const uint64_t periodNs = static_cast<uint64_t>(periodUs_) * 1000ULL;
    // Threshold for a "missed deadline": wakeup more than 10% late
    const uint64_t missThresholdNs = periodNs / 10;

    struct timespec next{};
    clock_gettime(CLOCK_MONOTONIC, &next);

    while (running_.load(std::memory_order_acquire)) {
        // Advance absolute wakeup time by one period
        next.tv_nsec += static_cast<long>(periodNs);
        while (next.tv_nsec >= 1000000000L) {
            next.tv_nsec -= 1000000000L;
            next.tv_sec  += 1;
        }

        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, nullptr);

        // Measure actual wakeup jitter
        struct timespec now{};
        clock_gettime(CLOCK_MONOTONIC, &now);

        const int64_t diffNs =
            (static_cast<int64_t>(now.tv_sec)  - static_cast<int64_t>(next.tv_sec))  * 1000000000LL
          + (static_cast<int64_t>(now.tv_nsec) - static_cast<int64_t>(next.tv_nsec));

        const uint64_t jitterUs = static_cast<uint64_t>(diffNs > 0 ? diffNs / 1000 : 0);

        if (static_cast<uint64_t>(diffNs > 0 ? diffNs : 0) > missThresholdNs) {
            missCount_.fetch_add(1, std::memory_order_relaxed);
        }

        uint64_t prev = worstJitterUs_.load(std::memory_order_relaxed);
        while (jitterUs > prev &&
               !worstJitterUs_.compare_exchange_weak(prev, jitterUs,
                   std::memory_order_relaxed, std::memory_order_relaxed)) {}

        jitterSum_.fetch_add(jitterUs, std::memory_order_relaxed);
        jitterSamples_.fetch_add(1,    std::memory_order_relaxed);

        callback_();
    }
}

uint64_t CyclicThread::getAverageJitterUs() const {
    const uint64_t samples = jitterSamples_.load(std::memory_order_relaxed);
    if (samples == 0) return 0;
    return jitterSum_.load(std::memory_order_relaxed) / samples;
}
