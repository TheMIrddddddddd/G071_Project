#ifndef SMOOTH_PORT_H
#define SMOOTH_PORT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
    /* One foreground snapshot for every channel. Reset epoch only after cancelling
     * all animations, at a scene boundary (<=12 s), keeping float seconds precise. */
    void SmoothPort_Init(uint64_t now_us);
    void SmoothPort_ResetEpoch(uint64_t now_us);
    uint32_t SmoothPort_Sample(uint64_t now_us);
    uint32_t SmoothPort_Millis(void);
#ifdef __cplusplus
}
#endif
#endif
