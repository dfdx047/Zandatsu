// The game process keeps its own log next to logcat: files/game.log in the app's
// private folder, and files/game-previous.log for the session before. Diagnostic
// reports are often sent after Android has dropped the game's lines from logcat
// (the process was killed, the phone restarted, hours went by), so most of them
// carried only the launcher's log. Diagnostics.java adds both files to its zip.
// Include once, from guest_runtime.cpp. DXVK's logger writes here too (weak symbol).
#pragma once
#include "runtime_log.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <sys/time.h>

namespace {
    FILE* openGameLog(std::string& path) {
        char command[256]={};
        if (FILE* input=std::fopen("/proc/self/cmdline","r")) { std::fread(command,1,sizeof command-1,input); std::fclose(input); }
        const char* game=std::strstr(command,":game");
        if (!game) return nullptr;  // only the game process (not the launcher or the GPU probe)
        std::string directory="/data/data/"+std::string(command,size_t(game-command))+"/files";
        mkdir(directory.c_str(),0700);
        path=directory+"/game.log";
        // Three sessions: players often start the game again once or twice after a
        // crash before sending the report, and the crashed session was gone.
        std::rename((directory+"/game-previous.log").c_str(),(directory+"/game-older.log").c_str());
        std::rename(path.c_str(),(directory+"/game-previous.log").c_str());
        FILE* file=std::fopen(path.c_str(),"w");
        if (file) setvbuf(file,nullptr,_IOLBF,8192);
        return file;
    }
}

extern "C" int nfsRuntimeLog(int priority,const char* tag,const char* format,...) {
    char text[4096];
    va_list arguments; va_start(arguments,format);
    int length=std::vsnprintf(text,sizeof text,format,arguments); va_end(arguments);
    __android_log_write(priority,tag,text);
    static std::mutex mutex; static FILE* file=nullptr; static bool opened=false; static long written=0; static std::string path;
    std::lock_guard<std::mutex> lock(mutex);
    if (!opened) { opened=true; file=openGameLog(path); }
    if (!file) return length;
    timeval now; gettimeofday(&now,nullptr); tm local; localtime_r(&now.tv_sec,&local);
    static const char levels[]="??VDIWEF";
    written+=std::fprintf(file,"%02d-%02d %02d:%02d:%02d.%03d %c %s: %s\n",local.tm_mon+1,local.tm_mday,local.tm_hour,local.tm_min,
        local.tm_sec,int(now.tv_usec/1000),priority>=0&&priority<8?levels[priority]:'?',tag,text);
    // Keep the beginning of a long session and its most recent part.
    if (written>(8l<<20)) { std::fclose(file); std::rename(path.c_str(),(path+".1").c_str()); file=std::fopen(path.c_str(),"w"); written=0;
        if (file) setvbuf(file,nullptr,_IOLBF,8192); }
    return length;
}
