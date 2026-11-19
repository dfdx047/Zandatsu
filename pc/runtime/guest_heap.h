#pragma once
#include "guest_memory.h"
#include <map>
#include <algorithm>
#include <vector>

// Guest heap addresses and opaque handles never depend on host malloc pointers.
// Calls are serialized by the guest machine lock.
class GuestHeaps {
    struct Block { uint32_t capacity, requested; };
    struct Heap { uint32_t options, maximum; uint64_t live{}; std::map<uint32_t,Block> blocks; };
    GuestMemory& memory_;
public:
#if UINTPTR_MAX > 0xffffffffu
    static constexpr uint32_t firstArena=0x10000000u,firstArenaEnd=0x1f000000u,secondArena=0x30000000u,secondArenaSize=0x40000000u;
#else
    // 32-bit host: two ~480 MB arenas around the 16 MB stack at 0x20000000,
    // all inside GuestMemory's 1 GB.
    static constexpr uint32_t firstArena=0x01000000u,firstArenaEnd=0x1f000000u,secondArena=0x22000000u,secondArenaSize=0x1e000000u;
#endif
private:
    std::map<uint32_t,uint32_t> free_{{firstArena,firstArenaEnd-firstArena},{secondArena,secondArenaSize}};
    std::map<uint32_t,Heap> heaps_;
    uint32_t next_=0xf0000104u;
    void release(uint32_t address,uint32_t capacity) {
        auto next=free_.lower_bound(address);
        if (next!=free_.begin()) {
            auto before=std::prev(next);
            if (uint64_t(before->first)+before->second==address) {
                address=before->first; capacity+=before->second; free_.erase(before);
            }
        }
        next=free_.lower_bound(address);
        if (next!=free_.end() && uint64_t(address)+capacity==next->first) {
            capacity+=next->second; free_.erase(next);
        }
        free_[address]=capacity;
    }
public:
    static constexpr uint32_t process=0xf0000100u;
    explicit GuestHeaps(GuestMemory& memory):memory_(memory) { heaps_.emplace(process,Heap{0,0,0,{}}); }
    uint32_t create(uint32_t options,uint32_t initial,uint32_t maximum) {
        if (maximum && initial>maximum) return 0;
        uint32_t id=next_; next_+=4; heaps_.emplace(id,Heap{options,maximum,0,{}}); return id;
    }
    bool destroy(uint32_t heap) {
        auto h=heaps_.find(heap); if (h==heaps_.end() || heap==process) return false;
        for (auto& b:h->second.blocks) release(b.first,b.second.capacity);
        heaps_.erase(h); return true;
    }
    uint32_t alloc(uint32_t heap,uint32_t flags,uint32_t bytes) {
        auto h=heaps_.find(heap); if(h==heaps_.end()) return 0;
        uint64_t capacity=(uint64_t(std::max(bytes,1u))+15)&~uint64_t(15);
        if (capacity>UINT32_MAX || (h->second.maximum &&
            (bytes>=0x7fff8u || h->second.live+capacity>h->second.maximum))) return 0;
        auto slot=std::find_if(free_.begin(),free_.end(),[&](auto& s){return s.second>=capacity;});
        if(slot==free_.end()) {
            if ((flags|h->second.options)&4) throw std::bad_alloc();
            return 0;
        }
        uint32_t address=slot->first, remaining=slot->second-static_cast<uint32_t>(capacity);
        // Windows heaps always leave committed memory after a block; game code
        // reads a little past string and file buffers (MW's localized-string
        // strncpy faulted at the next, uncommitted page on several phones).
        // Commit up to a page of the free space that follows as slack.
        memory_.commit(address,capacity+std::min<uint32_t>(remaining,4096u));
        free_.erase(slot); if(remaining) free_[address+static_cast<uint32_t>(capacity)]=remaining;
        h->second.blocks[address]={static_cast<uint32_t>(capacity),bytes}; h->second.live+=capacity;
        if(flags&8) std::memset(memory_.address(address,capacity),0,capacity);
        return address;
    }
    bool free(uint32_t heap,uint32_t address) {
        auto h=heaps_.find(heap); if(h==heaps_.end()) return false;
        if(!address) return true;
        auto b=h->second.blocks.find(address); if(b==h->second.blocks.end()) return false;
        release(address,b->second.capacity); h->second.live-=b->second.capacity; h->second.blocks.erase(b); return true;
    }
    uint32_t size(uint32_t heap,uint32_t address) const {
        auto h=heaps_.find(heap); if(h==heaps_.end()) return UINT32_MAX;
        auto b=h->second.blocks.find(address); return b==h->second.blocks.end()?UINT32_MAX:b->second.capacity;
    }
    uint32_t realloc(uint32_t heap,uint32_t flags,uint32_t address,uint32_t bytes) {
        auto h=heaps_.find(heap); if(h==heaps_.end()) return 0;
        auto b=h->second.blocks.find(address); if(b==h->second.blocks.end()) return 0;
        auto old=b->second;
        if(h->second.maximum && bytes>=0x7fff8u) return 0;
        if(bytes<=old.capacity) {
            if((flags&8) && bytes>old.requested) std::memset(memory_.address(address+old.requested,bytes-old.requested),0,bytes-old.requested);
            b->second.requested=bytes; return address;
        }
        uint64_t capacity=(uint64_t(bytes)+15)&~uint64_t(15);
        if(capacity>UINT32_MAX) return 0;
        auto adjacent=free_.find(address+old.capacity);
        if(adjacent!=free_.end() && adjacent->second>=capacity-old.capacity &&
            (!h->second.maximum || h->second.live+capacity-old.capacity<=h->second.maximum)) {
            uint32_t remaining=adjacent->second-static_cast<uint32_t>(capacity-old.capacity);
            memory_.commit(address,capacity+std::min<uint32_t>(remaining,4096u)); free_.erase(adjacent);
            if(remaining) free_[address+static_cast<uint32_t>(capacity)]=remaining;
            if(flags&8) std::memset(memory_.address(address+old.requested,bytes-old.requested),0,bytes-old.requested);
            h->second.live+=capacity-old.capacity; b->second={static_cast<uint32_t>(capacity),bytes}; return address;
        }
        if(flags&16) return 0;
        uint32_t moved=alloc(heap,flags,bytes); if(!moved) return 0;
        std::memcpy(memory_.address(moved,bytes),memory_.address(address,old.requested),std::min(old.requested,bytes));
        free(heap,address); return moved;
    }
};

inline void testGuestHeaps(GuestMemory& memory) {
    GuestHeaps h(memory);
    auto heap=h.create(0,0,0), other=h.create(0,0,0);
    auto a=h.alloc(heap,8,31), barrier=h.alloc(heap,0,64);
    auto* p=static_cast<unsigned char*>(memory.address(a,31));
    for(unsigned i=0;i<31;++i) { if(p[i]) throw std::runtime_error("Heap zero initialization failed"); p[i]=static_cast<unsigned char>(i+1); }
    if(h.free(other,a) || h.realloc(heap,16,a,4096)) throw std::runtime_error("Heap ownership/in-place semantics failed");
    auto b=h.realloc(heap,8,a,4096); if(!b || b==a) throw std::runtime_error("Heap moved growth failed");
    auto* q=static_cast<unsigned char*>(memory.address(b,4096));
    for(unsigned i=0;i<4096;++i) if(q[i]!=(i<31?i+1:0)) throw std::runtime_error("Heap realloc preservation/zero tail failed");
    if(!h.free(heap,b) || h.free(heap,b) || !h.free(heap,barrier) || !h.destroy(heap)) throw std::runtime_error("Heap free/destroy failed");
    auto reused=h.alloc(other,0,31);
    if(reused!=a) throw std::runtime_error("Heap freed range reuse failed");
    h.destroy(other);
}
