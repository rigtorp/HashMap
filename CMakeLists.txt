cmake_minimum_required(VERSION 3.2)    
project(HashMap)

set(CMAKE_EXPORT_COMPILE_COMMANDS "true")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wextra -pedantic -march=native")


add_subdirectory(vendor/benchmark)

add_executable(HashMapBenchmark HashMapBenchmark.cpp)

add_executable(HashMapBenchmark2 HashMapBenchmark2.cpp)
target_link_libraries(HashMapBenchmark2 benchmark)

add_executable(HashMapExample HashMapExample.cpp)
add_executable(HashMapTest HashMapTest.cpp)

enable_testing()
add_test(HashMapTest HashMapTest)


add_executable(HashMapTest2 HashMapTest2.cpp)
add_test(HashMapTest2 HashMapTest2)

