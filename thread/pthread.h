#pragma once

#include <pthread.h>
#include <atomic>
#include <exception>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include "thread.h"

class PThread : public Thread{
private:
	pthread_t thread_id_;
public:
	PThread() : thread_id_(0) {}
	bool start_thread() {
		if (thread_id_ != 0) {
			std::cerr << "[ERROR] already start thread "
			<< "(PThread::start_thread) " << '\n';
			return false;
		}
		if (setup() == false) {
			cleanup();
			return false;
		}
		if (pthread_create(&thread_id_, nullptr, thread_func, this) != 0) {
			std::cerr << "[ERROR] pthread_create PThread : "
			<< strerror(errno)
			<< "(PThread::start_thread) " << '\n';
			cleanup();
			return false;
		}
		return true;
	}
	void stop_thread() {
		if (thread_id_ != 0) {
			if (!thread_term_.load()) { thread_term_.store(true); }
			if (pthread_join(thread_id_, nullptr) != 0) {
				std::cerr << "[ERROR] pthread_join : " << strerror(errno) << " "
				<< "(PThread::start_thread) " << '\n';
			}
			thread_id_ = 0;
		}
		cleanup();
	}
	uint64_t get_thread_id() override {
        if (thread_id_ == 0) return 0;
        std::hash<pthread_t> hasher;
        return static_cast<uint64_t>(hasher(thread_id_));
    }
	virtual ~PThread() {};
private:
	virtual bool setup() = 0;
	virtual void cleanup() = 0;
	virtual void thread_loop() = 0;
};
