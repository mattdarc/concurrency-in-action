#include "ThreadPool.hpp"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <list>
#include <numeric>
#include <random>
#include <string_view>
#include <utility>

#include <benchmark/benchmark.h>

namespace {
ThreadPool pool;
}

template <typename T>
void print_list(std::string_view name, const std::list<T> &l) {
    std::cout << "List " << name << ":\n";
    for (const T &t : l) {
        std::cout << t << '\n';
    }
    std::cout << '\n';
}

struct NoopExecutor {};

struct PoolExecutor {
    template <typename Callable, typename... Args>
    decltype(auto) schedule(Callable &&f, Args &&...args) {
        return pool.enqueue([&]() { return f(std::forward<Args>(args)...); });
    }
};

struct AsyncExecutor {
    template <typename Callable, typename... Args>
    decltype(auto) schedule(Callable &&f, Args &&...args) {
        return std::async(std::launch::async, std::forward<Callable>(f),
                          std::forward<Args>(args)...);
    }
};

struct SerialExecutor {
    template <typename Callable, typename... Args>
    decltype(auto) schedule(Callable &&f, Args &&...args) {
        std::promise<std::result_of_t<Callable(Args...)>> prom;
        auto fut = prom.get_future();
        prom.set_value(f(std::forward<Args>(args)...));
        return fut;
    }
};

template <typename Executor, typename T>
std::list<T> concurrent_qsort(Executor &exec, std::list<T> input) {
    if (input.empty())
        return input;

    std::list<T> result;
    result.splice(result.begin(), input, input.begin());

    const T& pivot = *result.begin();

    auto partition_pt =
        std::partition(input.begin(), input.end(),
                       [&](const T &other) { return other < pivot; });

    std::list<T> lower;
    lower.splice(lower.end(), input, input.begin(), partition_pt);

    auto new_lower = exec.schedule(
        [&]() { return concurrent_qsort(exec, std::move(lower)); });

    result.splice(result.end(), concurrent_qsort(exec, std::move(input)));
    result.splice(result.begin(), new_lower.get());

    return result;
}

template <typename T> std::vector<T> generate_random(size_t size) {
    std::vector<int> random(size);
    std::iota(random.begin(), random.end(), 1);
    std::shuffle(random.begin(), random.end(),
                 std::mt19937{std::random_device{}()});

    return random;
}

void shuffle_list(std::list<int> &l, const std::vector<int> &random) {
    auto it = l.begin();
    for (auto val : random)
        *(it++) = val;
}

constexpr size_t SIZE = 200'000;

template <typename Executor> void BM_QSort(benchmark::State &st) {
    std::list<int> input(st.range(0));
    const auto random = generate_random<int>(st.range(0));

    if constexpr (std::is_same_v<Executor, PoolExecutor>) {
        pool.reset(st.range(1));
    }

    Executor e;

    while (st.KeepRunning()) {
        shuffle_list(input, random);
        input = concurrent_qsort(e, std::move(input));
    }
}

void BM_ShuffleBaseline(benchmark::State &st) {
    std::list<int> input(st.range(0));
    const auto random = generate_random<int>(st.range(0));

    while (st.KeepRunning()) {
        shuffle_list(input, random);
    }
}

void BM_StdSort(benchmark::State &st) {
    std::list<int> input(st.range(0));
    const auto random = generate_random<int>(st.range(0));

    while (st.KeepRunning()) {
        shuffle_list(input, random);
        input.sort();
    }
}

BENCHMARK(BM_ShuffleBaseline)->Arg(SIZE);
BENCHMARK(BM_StdSort)->Arg(SIZE);
BENCHMARK_TEMPLATE(BM_QSort, SerialExecutor)->Arg(SIZE);
BENCHMARK_TEMPLATE(BM_QSort, PoolExecutor)->ArgPair(SIZE, 4);
BENCHMARK_TEMPLATE(BM_QSort, PoolExecutor)->ArgPair(SIZE, 8);
BENCHMARK_TEMPLATE(BM_QSort, PoolExecutor)->ArgPair(SIZE, 12);
//BENCHMARK_TEMPLATE(BM_QSort, AsyncExecutor)->Arg(SIZE);
