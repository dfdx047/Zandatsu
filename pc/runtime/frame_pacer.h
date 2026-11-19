#pragma once
#include <chrono>
#include <algorithm>

// Missed deadlines are discarded. Replaying an old schedule produces bursts
// of uncapped frames followed by a full-period stall on slower devices.
class FramePacer {
public:
    using Clock=std::chrono::steady_clock;
    Clock::time_point deadline(Clock::time_point now,unsigned limit){
        if(!limit){limit_=0;next_={};return now;}
        if(limit!=limit_){limit_=limit;next_=now;}
        auto target=std::max(now,next_);
        next_=target+std::chrono::nanoseconds(std::max(1ull,1000000000ull/limit));
        return target;
    }
private:
    unsigned limit_=0;
    Clock::time_point next_{};
};
