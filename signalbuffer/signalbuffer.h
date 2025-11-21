#pragma once
#include <memory>
#include "sharedbuffer.h"

class SignalBuffer {
protected:
    std::unique_ptr<SharedBuffer> shared_buf_; 

public:
    SignalBuffer(std::unique_ptr<SharedBuffer> shared_buf) : shared_buf_(std::move(shared_buf)) {}
    virtual ~SignalBuffer() = default;
    // enqueue 시 신호를 보내는 함수
    virtual int32_t enqueue_wake(const uint8_t* data, size_t len) = 0;
    // 대기 중인 컨슈머 모두 깨우기
    virtual void wake_all() = 0;
    // dequeue 대기
    virtual int32_t dequeue_wait(uint8_t* out, size_t len) = 0;
};