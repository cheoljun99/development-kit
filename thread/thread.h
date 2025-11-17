#pragma once
#include <atomic>
#include <iostream>

class Thread {
protected:
    std::atomic<bool> thread_term_;
public:
    Thread() : thread_term_(false) {}
    bool get_thread_term() { return thread_term_.load(); }
    virtual ~Thread() {}
    virtual bool start_thread() = 0;
    virtual void stop_thread() = 0;
    virtual uint64_t get_thread_id() = 0;
protected:
    static void* thread_func(void* arg) {
        Thread* self = static_cast<Thread*>(arg);
        std::cout << "thread(ID : " << self->get_thread_id() << ") start...\n";
        try {
            self->thread_loop();
        } catch (const std::exception& e) {
            std::cerr << "[EXCEPT] thread exception: " << e.what() << '\n';
            self->thread_term_.store(true);
        }
        std::cout << "thread(ID : " << self->get_thread_id() << ") stop!!!\n";
        return nullptr;
    }
    virtual bool setup() = 0;
    virtual void cleanup() = 0;
    virtual void thread_loop() = 0;
};