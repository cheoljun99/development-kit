
#pragma once

#include "pthread.h"

class PWorker : public PThread{
public:
	~PWorker() override {
        stop_thread();
    }
private:
	bool setup() override;
	void cleanup() override;
	void thread_loop() override;
};
