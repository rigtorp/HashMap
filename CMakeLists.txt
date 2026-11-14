cmake_minimum_required(VERSION 3.24...4.3)

project(HashMap
  VERSION 1.0.0
  HOMEPAGE_URL "https://github.com/rigtorp/HashMap"
  LANGUAGES CXX
)

# -----------------------------------------------------------------------------
# Library Target
# -----------------------------------------------------------------------------
add_library(HashMap INTERFACE)
add_library(HashMap::HashMap ALIAS HashMap)
target_compile_features(HashMap INTERFACE cxx_std_14)
target_sources(HashMap INTERFACE
  FILE_SET HEADERS
  BASE_DIRS include
  FILES include/rigtorp/HashMap.h
)

# -----------------------------------------------------------------------------
# Tests, Examples & Benchmarks (Top-Level Only)
# -----------------------------------------------------------------------------
include(CTest)

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
  function(hashmap_executable target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE HashMap::HashMap)
    set_target_properties(${target} PROPERTIES
      CXX_EXTENSIONS OFF
      COMPILE_WARNING_AS_ERROR ON
    )
    target_compile_options(${target} PRIVATE
      "$<$<CXX_COMPILER_ID:MSVC>:/permissive-;/W4;/w14242>"
      "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic;-Wshadow;-Wconversion;-Wsign-conversion;-Wold-style-cast;-Wundef>"
    )
  endfunction()

  hashmap_executable(HashMapExample src/HashMapExample.cpp)

  hashmap_executable(HashMapTest src/HashMapTest.cpp)
  add_test(NAME HashMapTest COMMAND HashMapTest)

  if(NOT WIN32)
    hashmap_executable(HashMapBenchmark src/HashMapBenchmark.cpp)
    target_compile_features(HashMapBenchmark PRIVATE cxx_std_17)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
      target_compile_options(HashMapBenchmark PRIVATE -msse4.2)
    endif()

    option(HASHMAP_BENCHMARK_WITH_ABSL "Use installed Abseil when available" ON)
    if(HASHMAP_BENCHMARK_WITH_ABSL)
      find_package(absl CONFIG QUIET)
      if(TARGET absl::flat_hash_map)
        target_link_libraries(HashMapBenchmark PRIVATE absl::flat_hash_map)
        target_compile_definitions(HashMapBenchmark PRIVATE HASHMAP_BENCHMARK_HAS_ABSL)
      endif()
    endif()

    option(HASHMAP_BENCHMARK_WITH_SPARSEHASH "Use installed sparsehash when available" ON)
    if(HASHMAP_BENCHMARK_WITH_SPARSEHASH)
      find_path(HASHMAP_SPARSEHASH_INCLUDE_DIR NAMES google/dense_hash_map)
      mark_as_advanced(HASHMAP_SPARSEHASH_INCLUDE_DIR)
      if(HASHMAP_SPARSEHASH_INCLUDE_DIR)
        target_include_directories(HashMapBenchmark SYSTEM PRIVATE "${HASHMAP_SPARSEHASH_INCLUDE_DIR}")
        target_compile_definitions(HashMapBenchmark PRIVATE HASHMAP_BENCHMARK_HAS_SPARSEHASH)
      endif()
    endif()

    add_test(NAME HashMapBenchmarkSmoke COMMAND HashMapBenchmark -t 4 -c 1000 -i 1000)
  endif()
endif()

# -----------------------------------------------------------------------------
# Installation & Package Export
# -----------------------------------------------------------------------------
if(PROJECT_IS_TOP_LEVEL)
  include(GNUInstallDirs)
  include(CMakePackageConfigHelpers)

  export(
    TARGETS HashMap
    NAMESPACE HashMap::
    FILE HashMapConfig.cmake
  )

  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/HashMapConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT
  )

  install(TARGETS HashMap EXPORT HashMapConfig FILE_SET HEADERS)

  install(
    EXPORT HashMapConfig
    NAMESPACE HashMap::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/HashMap"
  )

  install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/HashMapConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/HashMap"
  )
endif()
