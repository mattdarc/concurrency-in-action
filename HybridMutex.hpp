#pragma once

#include <atomic>
#include <mutex>
#include <condition_variable>

class HybridMutex {
  public:
    HybridMutex();

    void lock() noexcept;
    void unlock() noexcept;
    bool try_lock() noexcept;

  private:
    std::mutex m_inner;
    std::condition_variable m_cv;
    std::atomic<bool> m_lock;

    bool spin() noexcept;
};
