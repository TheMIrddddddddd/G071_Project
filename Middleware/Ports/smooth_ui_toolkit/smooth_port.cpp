#include "smooth_port.h"
#include "core/hal/hal.hpp"
#include <cstdlib>
static uint64_t epoch_us;
static uint32_t sampled_ms;
void SmoothPort_Init(uint64_t now_us)
{
    SmoothPort_ResetEpoch(now_us);
    smooth_ui_toolkit::ui_hal::on_get_tick(SmoothPort_Millis);
    // No desktop sleep and no silent delay stub: blocking use violates the port.
    smooth_ui_toolkit::ui_hal::on_delay([](uint32_t) { std::abort(); });
}
void SmoothPort_ResetEpoch(uint64_t now_us)
{
    epoch_us = now_us;
    sampled_ms = 0;
}
uint32_t SmoothPort_Sample(uint64_t now_us)
{
    if (now_us < epoch_us || now_us - epoch_us > 60000000ULL)
        std::abort();
    sampled_ms = static_cast<uint32_t>((now_us - epoch_us) / 1000U);
    return sampled_ms;
}
uint32_t SmoothPort_Millis(void) { return sampled_ms; }
