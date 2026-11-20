// CPU placement for the guest. Guest threads run one at a time under the machine
// lock, so each one looks lightly loaded to the kernel scheduler even when together
// they fill a core. MediaTek's scheduler then leaves them on the little cores:
// a Dimensity 8300 reported 14 FPS at 360p, where the GPU cannot be the limit, while
// Snapdragons with less CPU kept 60. Guest threads stay on the big cores, and the
// frame thread tells the system its frame times (ADPF, Android 12+).
#pragma once
#include "runtime_log.h"
#include <dlfcn.h>
#include <sched.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <mutex>
#include <vector>
#include <cstdlib>
#include <cerrno>
#include <algorithm>

inline long readCpuValue(unsigned cpu,const char* file){
    char path[96];std::snprintf(path,sizeof path,"/sys/devices/system/cpu/cpu%u/%s",cpu,file);
    long value=-1;
    if(FILE* input=std::fopen(path,"r")){if(std::fscanf(input,"%ld",&value)!=1)value=-1;std::fclose(input);}
    return value;
}
// Cores with at least 60% of the fastest one; empty when that is all of them or
// the kernel does not say (nothing to choose, so the affinity is left alone).
inline const cpu_set_t& performanceCores(bool* useful=nullptr){
    static bool any=false;
    static const cpu_set_t set=[]{
        constexpr unsigned maximum=32;long speed[maximum];unsigned online=0;long fastest=0;
        const char* file="cpu_capacity";
        if(readCpuValue(0,file)<=0)file="cpufreq/cpuinfo_max_freq";
        std::string report;
        for(unsigned cpu=0;cpu<maximum;++cpu){
            speed[cpu]=readCpuValue(cpu,file);
            if(speed[cpu]<=0)continue;
            ++online;fastest=std::max(fastest,speed[cpu]);
            report+=(report.empty()?"":" ")+std::to_string(speed[cpu]);
        }
        cpu_set_t chosen;CPU_ZERO(&chosen);unsigned count=0;
        for(unsigned cpu=0;cpu<maximum;++cpu)if(speed[cpu]>0&&speed[cpu]*10>=fastest*6){CPU_SET(cpu,&chosen);++count;}
        // Only with four or more fast cores. On 2+6 designs (Helio G85, MT6769:
        // capacities 516x6, 1024x2) every guest thread squeezed onto two cores and
        // the game ran slower than when Android placed them (U1/U2 reports).
        any=count>=4&&count<online;
        NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","CPU cores (%s): %s; guest threads on %u of %u",file,report.c_str(),any?count:online,online);
        return chosen;
    }();
    if(useful)*useful=any;
    return set;
}
// Every guest thread's tid, for the ADPF session (lab MGR_ADPF_ALL=1, lesson from the Naruto Storm 4 port: the
// system raises the clocks for the threads that do the frame's work, not only the render thread).
inline std::mutex& guestTidLock(){static std::mutex m;return m;}
inline std::vector<int32_t>& guestTids(){static std::vector<int32_t> v;return v;}
inline void registerGuestTid(){std::lock_guard<std::mutex> l(guestTidLock());guestTids().push_back(gettid());}
// Lab MGR_AFFINITY_BIG=1: the calling thread (render/present) only on the fastest cluster.
inline void stayOnFastestCores(){
    static const bool on=std::getenv("MGR_AFFINITY_BIG")!=nullptr;if(!on)return;
    constexpr unsigned maximum=32;long speed[maximum];long fastest=0;
    for(unsigned cpu=0;cpu<maximum;++cpu){speed[cpu]=readCpuValue(cpu,"cpufreq/scaling_max_freq");fastest=std::max(fastest,speed[cpu]);}
    cpu_set_t set;CPU_ZERO(&set);unsigned n=0;
    for(unsigned cpu=0;cpu<maximum;++cpu)if(speed[cpu]>0&&speed[cpu]==fastest){CPU_SET(cpu,&set);++n;}
    int r=n?sched_setaffinity(0,sizeof set,&set):-1;
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","render thread on the %u fastest cores (%ld kHz): %s",n,fastest,r==0?"ok":"refused");
}
inline void stayOnPerformanceCores(){
    bool useful=false;const cpu_set_t& set=performanceCores(&useful);
    if(useful&&sched_setaffinity(0,sizeof set,&set))
        NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"NFSU2","Guest thread affinity refused: errno %d",errno);
}

// APerformanceHint is API 33 in the NDK headers' sense; this APK starts at API 29.
class FrameWorkHint {
    using Manager=struct APerformanceHintManager;
    using Session=struct APerformanceHintSession;
    int (*report_)(Session*,int64_t)=nullptr;
    Session* session_=nullptr;
    bool tried_=false;
    int (*setThreads_)(Session*,const int32_t*,size_t)=nullptr;
    int32_t self_=0;bool all_=false;unsigned frames_=0;size_t sent_=1;
public:
    void reportFrame(int64_t workNanoseconds,int64_t targetNanoseconds){
        if(!tried_){
            tried_=true;
            void* library=dlopen("libandroid.so",RTLD_NOW|RTLD_LOCAL);
            auto getManager=library?reinterpret_cast<Manager*(*)()>(dlsym(library,"APerformanceHint_getManager")):nullptr;
            auto create=library?reinterpret_cast<Session*(*)(Manager*,const int32_t*,size_t,int64_t)>(dlsym(library,"APerformanceHint_createSession")):nullptr;
            report_=library?reinterpret_cast<int(*)(Session*,int64_t)>(dlsym(library,"APerformanceHint_reportActualWorkDuration")):nullptr;
            setThreads_=library?reinterpret_cast<int(*)(Session*,const int32_t*,size_t)>(dlsym(library,"APerformanceHint_setThreads")):nullptr;
            int32_t self=gettid();self_=self;all_=std::getenv("MGR_ADPF_ALL")!=nullptr;
            Manager* manager=getManager?getManager():nullptr;
            if(manager&&create&&report_)session_=create(manager,&self,1,targetNanoseconds);
            NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","ADPF frame hints: %s%s",session_?"on":"not available",all_?" (all guest threads)":"");
            stayOnFastestCores();
        }
        if(all_&&session_&&setThreads_&&++frames_%120==1){
            std::vector<int32_t> tids;{std::lock_guard<std::mutex> l(guestTidLock());tids=guestTids();}
            if(std::find(tids.begin(),tids.end(),self_)==tids.end())tids.push_back(self_);
            if(tids.size()!=sent_){int r=setThreads_(session_,tids.data(),tids.size());sent_=tids.size();
                NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","ADPF session threads: %zu (%s)",tids.size(),r==0?"ok":"refused");}
        }
        if(session_&&workNanoseconds>0)report_(session_,workNanoseconds);
    }
};
