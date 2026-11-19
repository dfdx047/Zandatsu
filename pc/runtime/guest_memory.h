#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <sys/mman.h>
#include <unistd.h>
#include <map>
#include <algorithm>

// The guest's virtual address space, independent of host pointer values.
// Reserve only: committed regions are explicitly made accessible on demand.
// A 64-bit host reserves the full 4 GB. A 32-bit host (armeabi-v7a) cannot:
// the Java heap and ART's boot image sit inside any 2 GB window, so it reserves
// 1 GB and GuestHeaps keeps every guest allocation below that.
class GuestMemory {
public:
#if UINTPTR_MAX > 0xffffffffu
    static constexpr uint64_t size = uint64_t{1} << 32;
#else
    static constexpr uint64_t size = uint64_t{1} << 30;
#endif
private:
    void* base_ = MAP_FAILED;
    std::map<uint64_t,uint64_t> committed_;
    std::map<uint64_t,uint32_t> protection_;
    uint64_t page_=static_cast<uint64_t>(sysconf(_SC_PAGESIZE));
    bool accessible(uint32_t va,uint64_t bytes,bool write)const{
        if(!bytes)return true;if(uint64_t(va)>=size||bytes>size-va)return false;
        auto region=committed_.upper_bound(va);if(region==committed_.begin())return false;
        --region;if(uint64_t(va)+bytes>region->second)return false;
        for(uint64_t page=uint64_t(va)/page_*page_;page<uint64_t(va)+bytes;page+=page_){
            auto found=protection_.find(page);uint32_t value=found==protection_.end()?4:found->second;
            if(write?(value!=4&&value!=8&&value!=0x40&&value!=0x80):(value==1||value==0x10))return false;
        }return true;
    }
public:
    GuestMemory() {
        base_ = mmap(nullptr, size_t(size), PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        if (base_ == MAP_FAILED) throw std::runtime_error("Guest address-space reservation failed");
    }
    GuestMemory(const GuestMemory&) = delete;
    GuestMemory& operator=(const GuestMemory&) = delete;
    ~GuestMemory() { if (base_ != MAP_FAILED) munmap(base_, size_t(size)); }
    void* base() const { return base_; }
    bool readable(uint32_t va,uint64_t bytes)const{
        return accessible(va,bytes,false);
    }
    bool writable(uint32_t va,uint64_t bytes)const{return accessible(va,bytes,true);}
    // x86 execute permissions are guest metadata. ARM64 never executes these bytes.
    bool protect(uint32_t va,uint64_t bytes,uint32_t flags,uint32_t& previous){
        if(!bytes||uint64_t(va)>=size||bytes>size-va)return false;
        if(flags!=1&&flags!=2&&flags!=4&&flags!=8&&flags!=0x10&&flags!=0x20&&flags!=0x40&&flags!=0x80)return false;
        uint64_t start=uint64_t(va)/page_*page_,end=(uint64_t(va)+bytes+page_-1)/page_*page_;
        auto region=committed_.upper_bound(start);if(region==committed_.begin())return false;
        --region;if(end>region->second)return false;
        int native=flags==1?PROT_NONE:PROT_READ;
        if(flags==4||flags==8||flags==0x40||flags==0x80)native|=PROT_WRITE;
        auto old=protection_.find(start);uint32_t first=old==protection_.end()?4:old->second;
        if(mprotect(static_cast<unsigned char*>(base_)+start,end-start,native))return false;
        for(uint64_t page=start;page<end;page+=page_)protection_[page]=flags;
        previous=first;return true;
    }
    void* address(uint32_t va, uint64_t bytes) const {
        if (uint64_t(va) >= size || bytes > size - va) throw std::out_of_range("Guest range outside the guest address space");
        return static_cast<unsigned char*>(base_) + va;
    }
    void commit(uint32_t va, uint64_t bytes) {
        if (!bytes) throw std::invalid_argument("Empty guest commit");
        address(va, bytes);
        const uint64_t page = static_cast<uint64_t>(sysconf(_SC_PAGESIZE));
        const uint64_t start = uint64_t(va) / page * page;
        const uint64_t end = (uint64_t(va) + bytes + page - 1) / page * page;
        // Keep page zero inaccessible to diagnose guest null pointers.
        if (!start) throw std::invalid_argument("Guest null page cannot be committed");
        // Heap blocks share host pages. Committing a neighboring block must
        // preserve protections already applied to an existing page.
        uint64_t cursor=start;auto existing=committed_.upper_bound(start);
        if(existing!=committed_.begin()){auto before=std::prev(existing);if(before->second>start)existing=before;}
        while(cursor<end){
            if(existing!=committed_.end()&&existing->first<=cursor){cursor=std::max(cursor,existing->second);++existing;continue;}
            uint64_t limit=existing==committed_.end()?end:std::min(end,existing->first);
            if(mprotect(static_cast<unsigned char*>(base_)+cursor,limit-cursor,PROT_READ|PROT_WRITE))throw std::runtime_error("Guest commit failed");
            cursor=limit;
        }
        uint64_t mergedStart=start,mergedEnd=end;
        auto region=committed_.lower_bound(start);
        if(region!=committed_.begin()){auto previous=std::prev(region);if(previous->second>=start)region=previous;}
        while(region!=committed_.end()&&region->first<=mergedEnd){mergedStart=std::min(mergedStart,region->first);mergedEnd=std::max(mergedEnd,region->second);region=committed_.erase(region);}
        committed_.emplace(mergedStart,mergedEnd);
    }
};

inline void testGuestMemory() {
    GuestMemory mem;
    // Exercise the game image range and addresses with the high guest bit set.
    for (uint32_t va : {0x00400000u, uint32_t(GuestMemory::size/2), uint32_t(GuestMemory::size-0x10000u)}) {
        mem.commit(va, 4);
        const uint32_t expected = va ^ 0x12345678u;
        std::memcpy(mem.address(va, 4), &expected, 4);
        uint32_t actual = 0; std::memcpy(&actual, mem.address(va, 4), 4);
        if (actual != expected) throw std::runtime_error("Guest memory round trip failed");
    }
    bool rejected = false;
    try { mem.address(uint32_t(GuestMemory::size-1), 2); } catch (const std::out_of_range&) { rejected = true; }
    if (!rejected) throw std::runtime_error("Guest wraparound was accepted");
    if(mem.readable(0,1)||mem.readable(0x30000000,1)||!mem.readable(0x400000,4)||mem.readable(uint32_t(GuestMemory::size-1),2))throw std::runtime_error("Guest committed range validation failed");
}
