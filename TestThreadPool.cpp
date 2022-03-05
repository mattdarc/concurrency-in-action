#include "ThreadPool.hpp"

#include <iostream>


namespace {
    ThreadPool pool;
}

void doWork(int i) { std::cout << "Hello there from " << i << '\n'; }

void doNonCopyableWork(std::unique_ptr<int> i) { std::cout << "Hello there from " << *i << '\n'; }

int doRecursiveWork(int i = 0) {
    if (i < 100) {
        auto result =
            pool.enqueue(doRecursiveWork, i + 1);
        std::cout << "On recursive invocation " << result.get() << '\n';
    }

    return i;
}

int main() {
    std::cout << "Testing ThreadPool...\n";

    pool.reset(4);

    for (int i = 0; i < 10; ++i) {
        auto token = pool.enqueue([i]() { doWork(-i); });
        token.get();
    }

    for (int i = 0; i < 500; ++i) {
        pool.enqueue([i]() { doWork(i); });
    }

    for (int i = 0; i < 500; ++i) {
        pool.enqueue([i]() { doNonCopyableWork(std::make_unique<int>(i)); });
    }

    doRecursiveWork();

    std::cout << "Done testing ThreadPool...\n";
    pool.shutdown();

    return 0;
}
