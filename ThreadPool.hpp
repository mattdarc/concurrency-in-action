#pragma once

#include "Task.hpp"
#include "HybridMutex.hpp"

#include <deque>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <queue>
#include <thread>

namespace detail {

[[maybe_unused]] inline void logMessage(const char *msg) {
    std::cout << "[LOG]: " << msg << '\n';
}

} // namespace detail

#if 0
#define THREAD_LOG(MSG) detail::logMessage(MSG)
#else
#define THREAD_LOG(MSG) (void)0;
#endif

// TODO: This thread pool doesn't work for re-entrant/recursive code. Needs
// to pop off the waiting thread since we are starved
//
// consider recursive function A(), and we have 4 threads:
//
//    A_0() -> A_1() -> A_2() -> A_3() -> ... A_n()
//
// A_n-1 depends on A_n, but A_n cannot run until A_n-1 gives up its spot
// on the thread pool... The best way to do this might be cooperative --
// We implement a stop-token that yields it's time on the one of the worker
// threads when it calls get

template <typename Callable, typename... Args>
using AsyncResult = std::future<std::result_of_t<Callable(Args...)>>;

class ThreadPool {
  public:
    using Mutex = HybridMutex;

    ThreadPool();
    ThreadPool(std::size_t nThreads);

    ThreadPool(ThreadPool &&other);
    ThreadPool(const ThreadPool &other) = delete;

    ~ThreadPool();
    
    void reset(std::size_t nThreads);

    template <typename Callable, typename... Args>
    AsyncResult<Callable, Args...> enqueue(Callable &&, Args &&...);

    Task dequeue(const std::unique_lock<Mutex> &lk);

    void shutdown();

  private:
    class Thread;
    friend class Thread;

    void initialize(std::size_t nThreads);
    static void run(ThreadPool &self);

    Mutex m_mut;
    std::condition_variable_any m_workQueued;
    std::vector<std::unique_ptr<Thread>> m_threads;
    std::deque<Task> m_work;
    const std::thread::id m_mainThread;
    std::size_t m_active;
    bool m_done;
};

template <typename Callable, typename... Args>
AsyncResult<Callable, Args...> ThreadPool::enqueue(Callable &&callable,
                                                   Args &&...args) {
    THREAD_LOG("Adding work to the work queue");
    auto task =
        std::packaged_task<std::result_of_t<Callable(Args...)>()>(std::bind(
            std::forward<Callable>(callable), std::forward<Args>(args)...));

    auto result = task.get_future();

    std::unique_lock lk(m_mut);
    if (std::this_thread::get_id() != m_mainThread &&
        m_active == m_threads.size()) {
        // We are enqueueing a task from a worker thread. To make forward
        // progress, we need to run this function immediately since the calling
        // thread could depend on it
        //
        // TODO: Other option -- start up a new thread when get is called if
        // forward progress is not being made (based on positions in the queue)
        lk.unlock();

        task();
        THREAD_LOG("Executed task in same thread");
    } else {
        m_work.emplace_back(std::move(task));
        lk.unlock();

        m_workQueued.notify_one();
        THREAD_LOG("Notified threads of available work");
    }


    return result;
}

