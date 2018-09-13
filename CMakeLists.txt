cmake_minimum_required(VERSION 3.2)    
project(HashMap CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS "true")
set(CMAKE_CXX_STANDARD 14)

add_compile_options(-Wall -Wextra -Wpedantic -Werror)

add_library(HashMap INTERFACE)
target_include_directories(HashMap INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

add_executable(HashMapBenchmark src/HashMapBenchmark.cpp)
target_link_libraries(HashMapBenchmark HashMap)

add_executable(HashMapExample src/HashMapExample.cpp)
target_link_libraries(HashMapExample HashMap)

add_executable(HashMapTest src/HashMapTest.cpp)
target_link_libraries(HashMapTest HashMap)

enable_testing()
add_test(HashMapTest HashMapTest)
