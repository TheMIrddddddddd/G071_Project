/**
 * @file frame_clock.hpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2025-12-21
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once
#include "core/hal/hal.hpp"

namespace smooth_ui_toolkit::games {

class FrameClock {
public:
    void tick()
    {
        const uint32_t now = ui_hal::get_tick();
        _current = static_cast<float>(now) / 1000.0f;
        _dt = _has_tick ? static_cast<float>(now - _last_tick) / 1000.0f : 0.0f;
        _last_tick = now;
        _has_tick = true;
    }

    float dt() const
    {
        return _dt;
    }
    float now() const
    {
        return _current;
    }

private:
    float _current = 0.0f;
    uint32_t _last_tick = 0;
    bool _has_tick = false;
    float _dt = 0.0f;
};

} // namespace smooth_ui_toolkit::games
