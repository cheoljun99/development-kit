
#pragma once
#include "stdthread.h"

class STDWorker : public STDThread {
public:
    ~STDWorker() override { stop_thread(); }
private:
    bool setup() override;
    void cleanup() override;
    void thread_loop() override;
};