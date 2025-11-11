#pragma once
#include <atomic>
#include <cstdint>
#include "SPMCBuf.h"

class AtomicNotifySPMC {
    SPMCBuf spmc_buf_;
    std::atomic<int> flag_{0};

public:
    explicit AtomicNotifySPMC(size_t size) : spmc_buf_(size) {}

    int32_t enqueue_wake(const uint8_t* data, size_t len) {
        int32_t n = spmc_buf_.enqueue(data, len);
        if (n > 0) {
            flag_.store(1, std::memory_order_relaxed);
            flag_.notify_one();  
        }
        return n; // -1: full
    }

    void wake_all() {
        flag_.store(1, std::memory_order_relaxed);
        flag_.notify_all();
    }

    int32_t dequeue_wait(uint8_t* out, size_t len) {
        int32_t n = spmc_buf_.dequeue(out, len);
        if (n >= 0) return n;
        flag_.store(0, std::memory_order_relaxed);
        flag_.wait(0);
        return -1;
    }
};
