/**
 * @file random.hpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2025-12-19
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once
#ifndef SMOOTH_UI_TOOLKIT_ENABLE_COLOR
#define SMOOTH_UI_TOOLKIT_ENABLE_COLOR 1
#endif
#if SMOOTH_UI_TOOLKIT_ENABLE_COLOR
#include "core/color/color.hpp"
#endif
#ifdef SMOOTH_UI_TOOLKIT_SINGLE_CONTEXT
#include "smooth_single_context.hpp"
#else
#include <mutex>
#endif
#include <random>
#include <vector>
#include <cassert>

#ifdef ESP_PLATFORM
#include <esp_random.h>
#endif

#ifndef SMOOTH_UI_TOOLKIT_SINGLE_CONTEXT
namespace smooth_platform { using Mutex = std::mutex; using LockGuard = std::lock_guard<Mutex>; }
#endif

namespace smooth_ui_toolkit {

class Random {
public:
    static Random& getInstance()
    {
        static Random instance;
        return instance;
    }

    int getInt(int a, int b)
    {
        smooth_platform::LockGuard lock(_mutex);
        if (a > b) {
            std::swap(a, b);
        }
        return std::uniform_int_distribution<int>{a, b}(_rng);
    }

    float getFloat(float a, float b)
    {
        smooth_platform::LockGuard lock(_mutex);
        if (a > b) {
            std::swap(a, b);
        }
        return std::uniform_real_distribution<float>{a, b}(_rng);
    }

    double getDouble(double a, double b)
    {
        smooth_platform::LockGuard lock(_mutex);
        if (a > b) {
            std::swap(a, b);
        }
        return std::uniform_real_distribution<double>{a, b}(_rng);
    }

#if SMOOTH_UI_TOOLKIT_ENABLE_COLOR
    color::Rgb_t getRgbColor()
    {
        color::Rgb_t color;
        color.r = static_cast<uint8_t>(getInt(0, 255));
        color.g = static_cast<uint8_t>(getInt(0, 255));
        color.b = static_cast<uint8_t>(getInt(0, 255));
        return color;
    }

#endif

    template <typename T>
    const T& choice(const std::vector<T>& vec)
    {
        assert(!vec.empty());
        smooth_platform::LockGuard lock(_mutex);
        std::uniform_int_distribution<size_t> dist(0, vec.size() - 1);
        return vec[dist(_rng)];
    }

private:
    std::mt19937 _rng;
    smooth_platform::Mutex _mutex;

    Random()
    {
#ifdef SMOOTH_UI_TOOLKIT_RANDOM_SEED
        _rng.seed(SMOOTH_UI_TOOLKIT_RANDOM_SEED);
#elif defined(ESP_PLATFORM)
        _rng.seed(esp_random());
#else
        std::random_device rd;
        _rng.seed(rd());
#endif
    }
};

} // namespace smooth_ui_toolkit
