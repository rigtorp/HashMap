cmake_minimum_required(VERSION 3.2)    
project(HashMap)

set(CMAKE_EXPORT_COMPILE_COMMANDS "true")
set(CMAKE_CXX_FLAGS "-std=c++11 -Wall")

add_executable(HashMapBenchmark HashMapBenchmark.cpp)
add_executable(HashMapExample HashMapExample.cpp)
add_executable(HashMapTest HashMapTest.cpp)

enable_testing()
add_test(HashMapTest HashMapTest)
