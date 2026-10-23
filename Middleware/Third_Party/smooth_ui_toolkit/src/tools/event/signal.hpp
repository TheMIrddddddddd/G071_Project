/**
 * @file signal.hpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2025-03-21
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once
#include <functional>
#include <vector>
#ifdef SMOOTH_UI_TOOLKIT_SINGLE_CONTEXT
#include "smooth_single_context.hpp"
#else
#include <mutex>
#endif
#include <algorithm>

#ifndef SMOOTH_UI_TOOLKIT_SINGLE_CONTEXT
namespace smooth_platform { using Mutex = std::mutex; using LockGuard = std::lock_guard<Mutex>; }
#endif

namespace smooth_ui_toolkit {

/**
 * @brief Observer pattern like godot's signal
 *
 * @tparam T
 */
template <typename... Args>
class Signal {
public:
    using SlotType = std::function<void(Args...)>;

    void emit(Args... args)
    {
        smooth_platform::LockGuard lock(_mutex);
        for (auto& [id, slot] : _slots) {
            slot(args...);
        }
    }

    size_t connect(const SlotType& slot)
    {
        smooth_platform::LockGuard lock(_mutex);
        size_t id = ++_last_id;
        _slots.push_back({id, slot});
        return id;
    }

    void disconnect(size_t id)
    {
        smooth_platform::LockGuard lock(_mutex);
        _slots.erase(std::remove_if(_slots.begin(),
                                    _slots.end(),
                                    [id](auto& pair) {
                                        return pair.first == id;
                                    }),
                     _slots.end());
    }

    void clear()
    {
        smooth_platform::LockGuard lock(_mutex);
        _slots.clear();
    }

private:
    std::vector<std::pair<size_t, SlotType>> _slots;
    size_t _last_id = 0;
    smooth_platform::Mutex _mutex;
};

template <>
class Signal<void> : public Signal<> {};

} // namespace smooth_ui_toolkit
