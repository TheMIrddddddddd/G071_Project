/**
 * @file event_queue.hpp
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
#include <queue>
#ifdef SMOOTH_UI_TOOLKIT_SINGLE_CONTEXT
#include "smooth_single_context.hpp"
#else
#include <mutex>
#endif

#ifndef SMOOTH_UI_TOOLKIT_SINGLE_CONTEXT
namespace smooth_platform { using Mutex = std::mutex; using LockGuard = std::lock_guard<Mutex>; }
#endif

namespace smooth_ui_toolkit {

/**
 * @brief Just a queue with a mutex
 *
 * @tparam T
 */
template <typename T>
class EventQueue {
public:
    smooth_platform::Mutex mutex;
    std::queue<T> events;

    void emit(const T& event)
    {
        smooth_platform::LockGuard lock(mutex);
        events.push(event);
    }

    void poll(std::function<void(const T&)> onEvent)
    {
        smooth_platform::LockGuard lock(mutex);
        while (!events.empty()) {
            onEvent(events.front());
            events.pop();
        }
    };

    T front()
    {
        smooth_platform::LockGuard lock(mutex);
        return events.front();
    }

    void pop()
    {
        smooth_platform::LockGuard lock(mutex);
        events.pop();
    }

    bool empty()
    {
        smooth_platform::LockGuard lock(mutex);
        return events.empty();
    }

    size_t size()
    {
        smooth_platform::LockGuard lock(mutex);
        return events.size();
    }
};

} // namespace smooth_ui_toolkit
