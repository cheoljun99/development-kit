#pragma once

#include "thread.h"
#include <iostream>
#include <deque>
#include <memory>

class ThreadPool{
protected:
    std::deque<std::unique_ptr<Thread>> threads_;
    std::atomic<bool> start_flag_;
public:
    ThreadPool() : start_flag_(false){}
    bool start_pool(){
        if(start_flag_.load()==true){
            std::cerr << "[ERROR] already start thread pool "
			<< "(WorkerPool::start_pool) " << '\n';
			return false;
        }
        start_flag_.store(true);
        for(int i=0; i < threads_.size();i++){ 
            if(threads_[i]->start_thread()==false){
                stop_pool();
                return false;
            }
        }
        return true;
    }
    void stop_pool(){
        if(start_flag_.load()==true){
            for(int i=0; i < threads_.size();++i){ 
                threads_[i]->stop_thread();
            }
            start_flag_.store(false);
        }
    }
    virtual bool monitor_pool() = 0;
    virtual ~ThreadPool(){}
};