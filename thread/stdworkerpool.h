
#pragma once

#include "threadpool.h"
#include "stdworker.h"
#include <iostream>
#include <deque>
#include <cstddef>
#include <atomic>

class STDWorkerPool : public ThreadPool{
public:
    STDWorkerPool(size_t cnt){
        for (size_t i = 0; i < cnt; ++i) {
            threads_.push_back(std::make_unique<STDWorker>());
        }
    }
    ~STDWorkerPool() {
        stop_pool();
    }
    bool monitor_pool(){
        if(start_flag_.load()==false){
            std::cerr << "[ERROR] don't start thread pool "
			<< "(STDWorkerPool::monitor_pool) " << '\n';
            return false;
        }
        size_t dead_cnt=0;
        size_t recovery_fail_cnt=0;
        for(size_t i=0;i<threads_.size();++i){
            if(threads_[i]->get_thread_term()){
                dead_cnt++;
                threads_[i]->stop_thread();
                if(threads_[i]->start_thread()==false){
                    recovery_fail_cnt++;
                }
            }
        }
        std::cout << "LIVE THREAD COUNT : "<< threads_.size()- dead_cnt<<" "<<"DEAD THREAD COUNT : "<< dead_cnt<<" "
        <<"RECOVERY SUCCESS THREAD COUNT : "<<dead_cnt-recovery_fail_cnt<<" "<<"RECOVERY FAIL THREAD COUNT : "<<recovery_fail_cnt<<" \n";
        if(recovery_fail_cnt>0){
            stop_pool();
            return false;
        }
        return true;
    }
};