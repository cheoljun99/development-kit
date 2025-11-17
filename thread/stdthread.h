
#pragma once
#include <thread>
#include <atomic>
#include <exception>
#include <iostream>
#include <cstring>
#include "thread.h"

class STDThread : public Thread {
private:
    std::thread thread_;
public:
    bool start_thread() {
        if (thread_.joinable()) {
            std::cerr << "[ERROR] already started thread "
                << "(STDThread::start_thread) "
                << "thread(ID : " << std::this_thread::get_id() << ")\n";
            return false;
        }
        if (!setup()) {
            cleanup();
            return false;
        }
        thread_ = std::thread(&STDThread::thread_func, this);
        return true;
    }
    void stop_thread() {
        if (thread_.joinable()) {
            if (!thread_term_.load()) {
                thread_term_.store(true);
            }
            thread_.join();
        }
        cleanup();
    }
    uint64_t get_thread_id() override {
        if (!thread_.joinable()) return 0;
        std::hash<std::thread::id> hasher;
        return static_cast<uint64_t>(hasher(thread_.get_id()));
    }
    virtual ~STDThread() {};
private:
    virtual bool setup() = 0;
    virtual void cleanup() = 0;
    virtual void thread_loop() = 0;
};