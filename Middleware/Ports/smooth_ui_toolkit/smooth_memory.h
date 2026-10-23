#ifndef SMOOTH_MEMORY_H
#define SMOOTH_MEMORY_H
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
    typedef struct
    {
        uint32_t allocations, frees, live_bytes, peak_bytes;
    } SmoothMemoryStats;
    SmoothMemoryStats SmoothMemory_GetStats(void);
    /* The demo preallocates all C++ objects before sealing. Subsequent new is an
     * invariant violation, throws bad_alloc and is caught by the UI boundary. */
    void SmoothMemory_Seal(void);
#ifdef __cplusplus
}
#endif
#endif
