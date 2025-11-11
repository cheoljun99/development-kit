#pragma once
#include <stdatomic.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <unistd.h>
#include <cstdint>
#include <climits>
#include "SPMCBuf.h"

class FutexSPMC {
    SPMCBuf spmc_buf_;
    alignas(4) atomic_int flag_;

public:
    FutexSPMC(size_t size) : spmc_buf_(size), flag_(0) {}

    int32_t enqueue_wake(const uint8_t* data, size_t len) {
        int32_t n = spmc_buf_.enqueue(data, len);
        if (n > 0) {
            atomic_store_explicit(&flag_, 1, memory_order_relaxed);
            syscall(SYS_futex, &flag_, FUTEX_WAKE, 1, nullptr, nullptr, 0);
        }
        return n; // -1: full
    }

    void wake_all() {
        atomic_store_explicit(&flag_, 1, memory_order_relaxed);
        syscall(SYS_futex, &flag_, FUTEX_WAKE, INT_MAX, nullptr, nullptr, 0);
    }

    int32_t dequeue_wait(uint8_t* out, size_t len) {
        int32_t n = spmc_buf_.dequeue(out, len);
        if (n >= 0) return n;

        atomic_store_explicit(&flag_, 0, memory_order_relaxed);
        syscall(SYS_futex, &flag_, FUTEX_WAIT, 0, nullptr, nullptr, 0);
        return -1;
    }
};
