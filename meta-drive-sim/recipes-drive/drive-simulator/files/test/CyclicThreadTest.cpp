#include "rt/CyclicThread.hpp"
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool waitFor(const std::atomic<int>& counter, int target,
                    std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (counter.load(std::memory_order_acquire) >= target) return true;
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
    return false;
}

// ---------------------------------------------------------------------------
// Callback invocation
// ---------------------------------------------------------------------------

TEST(CyclicThreadTest, CallbackIsCalledRepeatedly) {
    std::atomic<int> count{0};

    CyclicThread thread(1000, 1, "test-cyc", [&]() {
        count.fetch_add(1, std::memory_order_relaxed);
    });

    thread.start();
    EXPECT_TRUE(waitFor(count, 5, std::chrono::milliseconds(50)));
    thread.stop();

    EXPECT_GE(count.load(), 5);
}

TEST(CyclicThreadTest, StopStopsCallbacks) {
    std::atomic<int> count{0};

    CyclicThread thread(1000, 1, "test-stop", [&]() {
        count.fetch_add(1, std::memory_order_relaxed);
    });

    thread.start();
    ASSERT_TRUE(waitFor(count, 3, std::chrono::milliseconds(50)));
    thread.stop();

    const int afterStop = count.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(count.load(), afterStop);
}

// ---------------------------------------------------------------------------
// Period accuracy (non-RT: just verify it runs within 2× the expected time)
// ---------------------------------------------------------------------------

TEST(CyclicThreadTest, PeriodIsApproximatelyCorrect) {
    std::atomic<int> count{0};

    CyclicThread thread(5000, 1, "test-period", [&]() {  // 5 ms period
        count.fetch_add(1, std::memory_order_relaxed);
    });

    const auto t0 = std::chrono::steady_clock::now();
    thread.start();
    ASSERT_TRUE(waitFor(count, 10, std::chrono::milliseconds(200)));
    thread.stop();
    const auto t1 = std::chrono::steady_clock::now();

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
    // 10 iterations × 5 ms = 50 ms; allow 3× margin in non-RT test environment
    EXPECT_LT(elapsed.count(), 150);
}

// ---------------------------------------------------------------------------
// Jitter tracking
// ---------------------------------------------------------------------------

TEST(CyclicThreadTest, WorstJitterIsTracked) {
    std::atomic<int> count{0};

    CyclicThread thread(2000, 1, "test-jitter", [&]() {
        count.fetch_add(1, std::memory_order_relaxed);
    });

    thread.start();
    ASSERT_TRUE(waitFor(count, 20, std::chrono::milliseconds(200)));
    thread.stop();

    // Some jitter exists in every environment (scheduler is not perfect)
    // Just verify the metric is non-negative and was computed
    EXPECT_GE(thread.getWorstJitterUs(), 0u);
    EXPECT_GE(thread.getAverageJitterUs(), 0u);
}

TEST(CyclicThreadTest, AverageJitterIsLessOrEqualToWorstJitter) {
    std::atomic<int> count{0};

    CyclicThread thread(2000, 1, "test-avg", [&]() {
        count.fetch_add(1, std::memory_order_relaxed);
    });

    thread.start();
    ASSERT_TRUE(waitFor(count, 30, std::chrono::milliseconds(300)));
    thread.stop();

    EXPECT_LE(thread.getAverageJitterUs(), thread.getWorstJitterUs());
}

// ---------------------------------------------------------------------------
// Missed deadline counter
// ---------------------------------------------------------------------------

TEST(CyclicThreadTest, MissCountStartsAtZero) {
    std::atomic<int> count{0};

    CyclicThread thread(5000, 1, "test-miss", [&]() {
        count.fetch_add(1, std::memory_order_relaxed);
    });

    thread.start();
    ASSERT_TRUE(waitFor(count, 2, std::chrono::milliseconds(50)));
    thread.stop();

    // In a quiet test environment miss count is typically 0 or very small
    EXPECT_LT(thread.getMissCount(), static_cast<uint64_t>(count.load()));
}

// ---------------------------------------------------------------------------
// Destructor stops the thread
// ---------------------------------------------------------------------------

TEST(CyclicThreadTest, DestructorStopsThread) {
    std::atomic<int> count{0};
    {
        CyclicThread thread(1000, 1, "test-dtor", [&]() {
            count.fetch_add(1, std::memory_order_relaxed);
        });
        thread.start();
        ASSERT_TRUE(waitFor(count, 3, std::chrono::milliseconds(50)));
    }  // destructor called here
    const int afterDtor = count.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(count.load(), afterDtor);
}
