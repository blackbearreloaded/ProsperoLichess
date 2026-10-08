// ProsperoLichess - Minimal pthread-based locking primitives for the network layer.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <cstdint>
#include <ctime>
#include <pthread.h>

namespace pch::net
{

// Busy-wait lock for a handful of instructions (no allocation, no pthread
// object), usable from statically allocated objects.
class SpinLock
{
  public:
    void lock()
    {
        while (flag_.test_and_set(std::memory_order_acquire))
        {
        }
    }
    void unlock()
    {
        flag_.clear(std::memory_order_release);
    }

  private:
    std::atomic_flag flag_;
};

class Mutex
{
  public:
    Mutex()
    {
        pthread_mutex_init(&mutex_, nullptr);
    }
    ~Mutex()
    {
        pthread_mutex_destroy(&mutex_);
    }
    Mutex(const Mutex &) = delete;
    Mutex &operator=(const Mutex &) = delete;

    void lock()
    {
        pthread_mutex_lock(&mutex_);
    }
    void unlock()
    {
        pthread_mutex_unlock(&mutex_);
    }
    pthread_mutex_t *native()
    {
        return &mutex_;
    }

  private:
    pthread_mutex_t mutex_;
};

template <class Lockable> class Guard
{
  public:
    explicit Guard(Lockable &lock) : lock_(lock)
    {
        lock_.lock();
    }
    ~Guard()
    {
        lock_.unlock();
    }
    Guard(const Guard &) = delete;
    Guard &operator=(const Guard &) = delete;

  private:
    Lockable &lock_;
};

class CondVar
{
  public:
    CondVar()
    {
        pthread_cond_init(&cond_, nullptr);
    }
    ~CondVar()
    {
        pthread_cond_destroy(&cond_);
    }
    CondVar(const CondVar &) = delete;
    CondVar &operator=(const CondVar &) = delete;

    void wait(Mutex &mutex)
    {
        pthread_cond_wait(&cond_, mutex.native());
    }
    // Waits at most timeout_ms (spurious wakeups possible; callers re-check).
    void wait_for_ms(Mutex &mutex, int timeout_ms)
    {
        timespec deadline{};
        clock_gettime(CLOCK_REALTIME, &deadline);
        const std::int64_t ns = static_cast<std::int64_t>(deadline.tv_nsec) +
                                static_cast<std::int64_t>(timeout_ms) * 1000000;
        deadline.tv_sec += static_cast<time_t>(ns / 1000000000);
        deadline.tv_nsec = static_cast<long>(ns % 1000000000);
        pthread_cond_timedwait(&cond_, mutex.native(), &deadline);
    }
    void signal()
    {
        pthread_cond_signal(&cond_);
    }
    void broadcast()
    {
        pthread_cond_broadcast(&cond_);
    }

  private:
    pthread_cond_t cond_;
};

} // namespace pch::net
