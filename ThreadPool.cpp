#include "ThreadPool.hpp"

#include <cassert>
#include <iostream>
#include <optional>

// Thin wrapper around thread that always runs, and uses a condition
// variable to signify when work is available
class ThreadPool::Thread {
  public:
    template <typename Callable>
    Thread(Callable &&callable) : m_thread(std::forward<Callable>(callable)) {
        THREAD_LOG("Created thread");
    }

    Thread(Thread &&other) = delete;
    Thread(const Thread &other) = delete;
    ~Thread() {
        THREAD_LOG("Terminating thread");
        m_thread.join();
    }

  private:
    std::thread m_thread;
};

ThreadPool::ThreadPool() : ThreadPool(std::thread::hardware_concurrency()) {}

void ThreadPool::initialize(std::size_t nThreads) {
    for (auto i = 0u; i < nThreads; ++i) {
        m_threads.push_back(
            std::make_unique<Thread>([this]() { ThreadPool::run(*this); }));
    }
    THREAD_LOG("Created all threads");
}

ThreadPool::ThreadPool(std::size_t nThreads)
    : m_mut(), m_workQueued(), m_threads(), m_work(),
      m_mainThread(std::this_thread::get_id()), m_active(0), m_done(false) {
    initialize(nThreads);
}

void ThreadPool::reset(std::size_t nThreads) {
    shutdown();

    m_done = false;
    initialize(nThreads);
}

Task ThreadPool::dequeue(
    [[maybe_unused]] const std::unique_lock<ThreadPool::Mutex> &lk) {
    assert(lk.mutex() == &m_mut && lk.owns_lock() &&
           "Lock must be owned by this thread!");
    auto task = std::move(m_work.front());
    m_work.pop_front();
    return task;
}

void ThreadPool::shutdown() {
    if (m_threads.empty()) {
        return;
    }

    {
        std::scoped_lock lk(m_mut);
        m_done = true;
    }

    m_workQueued.notify_all();
    m_threads.clear();
}

ThreadPool::~ThreadPool() { shutdown(); }

void ThreadPool::run(ThreadPool &self) {
    THREAD_LOG("Starting thread runner");
    std::unique_lock lk(self.m_mut);
    while (true) {
        self.m_workQueued.wait(
            lk, [&]() { return self.m_done || !self.m_work.empty(); });
        if (self.m_done)
            return;

        ++self.m_active;
        THREAD_LOG("Woken up to run work");
        auto task = self.dequeue(lk);
        lk.unlock();

        task();

        THREAD_LOG("Finished running work");
        lk.lock();
        --self.m_active;
    }
}
