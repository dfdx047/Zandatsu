#pragma once
#include "guest_runtime.h"
#include <pthread.h>
#include <mutex>
#include <string>
#include <stdexcept>

class GuestBootWorker {
    pthread_t thread_{};
    bool started_=false;
    std::mutex mutex_;
    std::string executable_,status_="Arranque CRT pendiente";
    static void* run(void* context) {
        auto& self=*static_cast<GuestBootWorker*>(context);
        auto result=connectGuestRuntime(self.executable_.c_str());
        std::lock_guard<std::mutex> guard(self.mutex_);self.status_=result;return nullptr;
    }
public:
    ~GuestBootWorker(){if(started_){requestGuestRuntimeStop();pthread_join(thread_,nullptr);}}
    void start(const std::string& executable) {
        if(started_)return;
        executable_=executable;
        {std::lock_guard<std::mutex> guard(mutex_);status_="Ejecutando entrada CRT del juego…";}
        pthread_attr_t attributes;
        if(pthread_attr_init(&attributes))throw std::runtime_error("Guest thread attributes failed");
        int result=pthread_attr_setstacksize(&attributes,64u<<20);
        if(!result)result=pthread_create(&thread_,&attributes,run,this);
        pthread_attr_destroy(&attributes);
        if(result)throw std::runtime_error("Guest worker creation failed: "+std::to_string(result));
        started_=true;
    }
    std::string status(){std::lock_guard<std::mutex> guard(mutex_);return status_;}
};
