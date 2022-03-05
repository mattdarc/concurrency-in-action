#pragma once

#include <memory>
#include <utility>

// Simple type-erased wrapper that works for non-copyable types
class Task {
  public:
    template <typename T>
    Task(T &&callable) : m_impl(new Callable<T>(std::forward<T>(callable))) {}
    void operator()() { m_impl->call(); }

  private:
    struct CallableBase {
        virtual void call() = 0;
        virtual ~CallableBase() = default;
    };

    template <typename CT> struct Callable : CallableBase {
        Callable(CT &&callable) : m_callable(std::forward<CT>(callable)) {}
        void call() final { m_callable(); }
        CT m_callable;
    };

    std::unique_ptr<CallableBase> m_impl;
};
