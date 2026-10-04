cmake_minimum_required(VERSION 3.20)

project(HashMap VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(HASHMAP_TOP_LEVEL OFF)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(HASHMAP_TOP_LEVEL ON)
endif()

option(HASHMAP_BUILD_TESTS "Build HashMap tests" ${HASHMAP_TOP_LEVEL})
option(HASHMAP_BUILD_EXAMPLES "Build HashMap examples" ${HASHMAP_TOP_LEVEL})
set(HASHMAP_BENCHMARK_DEFAULT ${HASHMAP_TOP_LEVEL})
if(WIN32)
    set(HASHMAP_BENCHMARK_DEFAULT OFF)
endif()
option(HASHMAP_BUILD_BENCHMARKS "Build the POSIX benchmark" ${HASHMAP_BENCHMARK_DEFAULT})
option(HASHMAP_INSTALL "Generate HashMap install rules" ${HASHMAP_TOP_LEVEL})
option(HASHMAP_WARNINGS_AS_ERRORS "Treat HashMap executable warnings as errors" OFF)

add_library(HashMap INTERFACE)
add_library(HashMap::HashMap ALIAS HashMap)
target_compile_features(HashMap INTERFACE cxx_std_14)
target_include_directories(HashMap INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

function(hashmap_executable target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE HashMap::HashMap)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    if(MSVC)
        target_compile_options(${target} PRIVATE /permissive- /W4)
        if(HASHMAP_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(HASHMAP_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

if(HASHMAP_BUILD_TESTS)
    include(CTest)
    if(BUILD_TESTING)
        hashmap_executable(HashMapTest src/HashMapTest.cpp)
        add_test(NAME HashMapTest COMMAND HashMapTest)
    endif()
endif()

if(HASHMAP_BUILD_EXAMPLES)
    hashmap_executable(HashMapExample src/HashMapExample.cpp)
endif()

if(HASHMAP_BUILD_BENCHMARKS)
    if(WIN32)
        message(FATAL_ERROR "HashMapBenchmark requires POSIX; disable HASHMAP_BUILD_BENCHMARKS on Windows")
    endif()

    option(HASHMAP_BENCHMARK_WITH_ABSL "Use installed Abseil when available" ON)
    option(HASHMAP_BENCHMARK_WITH_SPARSEHASH "Use installed sparsehash when available" ON)
    hashmap_executable(HashMapBenchmark src/HashMapBenchmark.cpp)
    target_compile_features(HashMapBenchmark PRIVATE cxx_std_17)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
        target_compile_options(HashMapBenchmark PRIVATE -msse4.2)
    endif()

    if(HASHMAP_BENCHMARK_WITH_ABSL)
        find_package(absl CONFIG QUIET)
        if(TARGET absl::flat_hash_map)
            target_link_libraries(HashMapBenchmark PRIVATE absl::flat_hash_map)
            target_compile_definitions(HashMapBenchmark PRIVATE HASHMAP_BENCHMARK_HAS_ABSL)
        endif()
    endif()
    if(HASHMAP_BENCHMARK_WITH_SPARSEHASH)
        find_path(HASHMAP_SPARSEHASH_INCLUDE_DIR NAMES google/dense_hash_map)
        mark_as_advanced(HASHMAP_SPARSEHASH_INCLUDE_DIR)
        if(HASHMAP_SPARSEHASH_INCLUDE_DIR)
            target_include_directories(HashMapBenchmark SYSTEM PRIVATE "${HASHMAP_SPARSEHASH_INCLUDE_DIR}")
            target_compile_definitions(HashMapBenchmark PRIVATE HASHMAP_BENCHMARK_HAS_SPARSEHASH)
        endif()
    endif()
    if(HASHMAP_BUILD_TESTS AND BUILD_TESTING)
        add_test(NAME HashMapBenchmarkSmoke COMMAND HashMapBenchmark -t 4 -c 1000 -i 1000)
    endif()
endif()

if(HASHMAP_INSTALL)
    set(HASHMAP_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/HashMap")
    configure_package_config_file(cmake/HashMapConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/HashMapConfig.cmake"
        INSTALL_DESTINATION "${HASHMAP_INSTALL_CMAKEDIR}")
    write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/HashMapConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
        ARCH_INDEPENDENT)

    install(TARGETS HashMap EXPORT HashMapTargets)
    install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    install(EXPORT HashMapTargets FILE HashMapTargets.cmake
        NAMESPACE HashMap:: DESTINATION "${HASHMAP_INSTALL_CMAKEDIR}")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/HashMapConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/HashMapConfigVersion.cmake"
        DESTINATION "${HASHMAP_INSTALL_CMAKEDIR}")
    export(EXPORT HashMapTargets FILE "${CMAKE_CURRENT_BINARY_DIR}/HashMapTargets.cmake"
        NAMESPACE HashMap::)
endif()
