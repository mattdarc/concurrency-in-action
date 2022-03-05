# Trying out concepts from Anthony Williams' Concurrency in Action
1. A simple thread pool with benchmarks for sorting a std::list

## Building
$ cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/benchmark ..

$ cmake --build .
