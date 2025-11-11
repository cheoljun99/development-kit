#pragma once
#include <mutex>
#include <condition_variable>
#include <cstdint>
#include "SPMCBuf.h"

class CVNotifySPMC {
    SPMCBuf buf_;
    std::mutex mtx_;
    std::condition_variable cv_;
    uint64_t gen_; 

public:
    CVNotifySPMC(size_t size): buf_(size), gen_(0) {}

    int32_t enqueue_wake(const uint8_t* data, size_t len) {
        std::unique_lock<std::mutex> lock(mtx_);
        int32_t n = buf_.enqueue(data, len);
        if (n > 0) {
            ++gen_;
            lock.unlock();
            cv_.notify_one();
        }
        return n; // -1: full
    }

    void wake_all() {
        std::unique_lock<std::mutex> lock(mtx_);
        ++gen_;
        lock.unlock();
        cv_.notify_all();
    }

    int32_t dequeue_wait(uint8_t* out, size_t len) {
        std::unique_lock<std::mutex> lock(mtx_);
        int32_t n = buf_.dequeue(out, len);
        if (n > 0) return n;
        uint64_t local_gen = gen_;
        while (local_gen==gen_) cv_.wait(lock);
        lock.unlock();
        return -1;
    }
};
