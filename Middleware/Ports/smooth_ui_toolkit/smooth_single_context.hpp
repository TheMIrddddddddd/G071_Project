#pragma once
#include <cstdlib>
// Bare-metal foreground contract: these containers are never shared with ISRs
// or other tasks. Detect reentrant use (including callbacks reentering a Signal)
// instead of silently pretending an empty lock provides synchronization.
namespace smooth_platform
{
class Mutex
{
    bool locked_ = false;

  public:
    void lock()
    {
        if (locked_)
            std::abort();
        locked_ = true;
    }
    void unlock()
    {
        if (!locked_)
            std::abort();
        locked_ = false;
    }
};
class LockGuard
{
    Mutex &mutex_;

  public:
    explicit LockGuard(Mutex &mutex) : mutex_(mutex) { mutex_.lock(); }
    ~LockGuard() { mutex_.unlock(); }
    LockGuard(const LockGuard &) = delete;
    LockGuard &operator=(const LockGuard &) = delete;
};
} // namespace smooth_platform
