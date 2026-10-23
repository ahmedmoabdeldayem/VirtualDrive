#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <pthread.h>
#include <string>

class CyclicThread {
public:
    // period_us: wakeup period in microseconds
    // priority:  SCHED_FIFO priority (1–99)
    // name:      thread name (max 15 chars for pthread_setname_np)
    // callback:  work to execute each period; must not allocate heap
    CyclicThread(uint32_t period_us,
                 int      priority,
                 std::string name,
                 std::function<void()> callback);
    ~CyclicThread();

    CyclicThread(const CyclicThread&)            = delete;
    CyclicThread& operator=(const CyclicThread&) = delete;

    void start();
    void stop();

    uint64_t getMissCount()       const { return missCount_.load(std::memory_order_relaxed); }
    uint64_t getWorstJitterUs()   const { return worstJitterUs_.load(std::memory_order_relaxed); }
    uint64_t getAverageJitterUs() const;

private:
    static void* threadEntry(void* arg);
    void run();

    uint32_t              periodUs_;
    int                   priority_;
    std::string           name_;
    std::function<void()> callback_;

    pthread_t             thread_     {0};
    std::atomic<bool>     running_    {false};

    std::atomic<uint64_t> missCount_     {0};
    std::atomic<uint64_t> worstJitterUs_ {0};
    std::atomic<uint64_t> jitterSum_     {0};
    std::atomic<uint64_t> jitterSamples_ {0};
};
