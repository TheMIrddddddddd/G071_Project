#include "smooth_memory.h"
#include <cstddef>
#include <cstdlib>
#include <new>
#include <cstdint>
namespace
{
struct alignas(std::max_align_t) Header { size_t bytes; bool used; };
constexpr size_t limit = 2048;
alignas(std::max_align_t) unsigned char arena[limit];
SmoothMemoryStats stats{};
bool sealed;
bool prepared;
void *allocate(size_t bytes)
{
    if (sealed || bytes > limit - sizeof(Header)) throw std::bad_alloc();
    if (!prepared) { new(arena) Header{limit,false}; prepared=true; }
    constexpr size_t alignment=alignof(std::max_align_t);
    const size_t total=((bytes?bytes:1)+sizeof(Header)+alignment-1)/alignment*alignment;
    for(size_t offset=0;offset<limit;) {
        auto *p=reinterpret_cast<Header *>(arena+offset);
        if(!p->used&&p->bytes>=total) {
            const size_t remaining=p->bytes-total;
            if(remaining>=sizeof(Header)+alignment) {
                new(arena+offset+total) Header{remaining,false};p->bytes=total;
            }
            p->used=true;++stats.allocations;stats.live_bytes+=static_cast<uint32_t>(p->bytes);
            if(stats.live_bytes>stats.peak_bytes)stats.peak_bytes=stats.live_bytes;
            return p+1;
        }
        offset+=p->bytes;
    }
    throw std::bad_alloc();
}
void release(void *ptr)
{
    if (!ptr)
        return;
    auto *p = static_cast<Header *>(ptr) - 1;
    stats.live_bytes -= static_cast<uint32_t>(p->bytes);
    ++stats.frees;
    p->used=false;
    for(size_t offset=0;offset<limit;) {
        auto *a=reinterpret_cast<Header *>(arena+offset);
        size_t next=offset+a->bytes;
        if(!a->used&&next<limit) {
            auto *b=reinterpret_cast<Header *>(arena+next);
            if(!b->used){a->bytes+=b->bytes;continue;}
        }
        offset+=a->bytes;
    }
}
} // namespace
void *operator new(size_t bytes) { return allocate(bytes); }
void *operator new[](size_t bytes) { return allocate(bytes); }
void operator delete(void *ptr) noexcept { release(ptr); }
void operator delete[](void *ptr) noexcept { release(ptr); }
void operator delete(void *ptr, size_t) noexcept { release(ptr); }
void operator delete[](void *ptr, size_t) noexcept { release(ptr); }
SmoothMemoryStats SmoothMemory_GetStats(void) { return stats; }
void SmoothMemory_Seal(void) { sealed = true; }
