cmake_minimum_required(VERSION 3.2)    
project(HashMap)

set(CMAKE_CXX_FLAGS "-std=c++14 -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

add_executable(benchmark benchmark.cpp)
add_executable(tests tests.cpp)

enable_testing()
add_test(test tests)