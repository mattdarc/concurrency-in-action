project(concurrency)

cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(
    -g -std=c++17 -Wall -Wextra -O3 -fno-omit-frame-pointer 
    -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free)
add_link_options(-ljemalloc -lpthread)

add_executable(TestThreadPool HybridMutex.cpp ThreadPool.cpp TestThreadPool.cpp)
target_link_libraries(TestThreadPool ${LD_COMMON_FLAGS})

find_package(benchmark REQUIRED PATHS $ENV{HOME}/benchmark/build)

add_executable(bench bench.cpp HybridMutex.cpp ThreadPool.cpp)
target_include_directories(bench PRIVATE benchmark::benchmark)
target_link_libraries(bench benchmark::benchmark benchmark::benchmark_main ${LD_COMMON_FLAGS})
