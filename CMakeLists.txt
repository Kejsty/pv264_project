cmake_minimum_required(VERSION 3.0)
project(pv264_project)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z -o example -pedantic -Wall -Wextra -pthread")


add_executable(queue_sharedPtr_basic lockfree/sharedPtr/example/basic.cpp)
add_executable(queue_sharedPtr_parallel lockfree/sharedPtr/example/parallel.cpp)

add_executable(queue_memPool_basic lockfree/memPool/example/basic.cpp)
add_executable(queue_memPool_parallel lockfree/memPool/example/parallel.cpp)
add_executable(queue_memPool_test tests/queue_memPool.cpp)


# benchmarks
add_executable(queue_benchmarks benchmarks/benchmark.cpp)