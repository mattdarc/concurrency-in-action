#include "HybridMutex.hpp"

namespace {
constexpr size_t kLoopCount = 2048;
}

HybridMutex::HybridMutex() : m_inner(), m_cv(), m_lock(false) {}

// Returns true if we acquired the lock while spinning
bool HybridMutex::spin() noexcept {
    for (;;) {
        // Optimistically assume the lock is free on the first try
        if (!m_lock.exchange(true, std::memory_order_acquire)) {
            break;
        }

        // Wait for lock to be released without generating cache misses
        for (size_t i = 0;
             i < kLoopCount && m_lock.load(std::memory_order_relaxed); ++i) {
            // Issue X86 PAUSE or ARM YIELD instruction to reduce contention
            // between hyper-threads
            __builtin_ia32_pause();
        }
    }

    return m_lock.load(std::memory_order::memory_order_acquire);
}

void HybridMutex::lock() noexcept {
    if (spin())
        return;

    std::unique_lock lk(m_inner);
    m_cv.wait(lk, [this]() {
        return !m_lock.exchange(true, std::memory_order::memory_order_acquire);
    });
}

void HybridMutex::unlock() noexcept {
    m_lock.store(false, std::memory_order_release);
    m_cv.notify_one();
}

bool HybridMutex::try_lock() noexcept {
    // First do a relaxed load to check if lock is free in order to prevent
    // unnecessary cache misses if someone does while(!try_lock())
    return !m_lock.load(std::memory_order_relaxed) &&
           !m_lock.exchange(true, std::memory_order_acquire);
}
