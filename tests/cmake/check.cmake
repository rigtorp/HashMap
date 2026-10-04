cmake_minimum_required(VERSION 3.20)

if(NOT HASHMAP_SOURCE_DIR OR NOT HASHMAP_CHECK_DIR)
    message(FATAL_ERROR "Set HASHMAP_SOURCE_DIR and HASHMAP_CHECK_DIR")
endif()

function(run)
    execute_process(COMMAND ${ARGV} COMMAND_ECHO STDOUT RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Command failed (${result}): ${ARGV}")
    endif()
endfunction()

function(check_consumer name)
    set(build "${HASHMAP_CHECK_DIR}/${name}")
    run("${CMAKE_COMMAND}" -S "${HASHMAP_SOURCE_DIR}/tests/cmake/consumer"
        -B "${build}" ${ARGN})
    run("${CMAKE_COMMAND}" --build "${build}" --config Debug --parallel 2)
    run("${CMAKE_CTEST_COMMAND}" --test-dir "${build}" -C Debug --output-on-failure)
endfunction()

set(library_build "${HASHMAP_CHECK_DIR}/library")
set(prefix "${HASHMAP_CHECK_DIR}/prefix")
run("${CMAKE_COMMAND}" -S "${HASHMAP_SOURCE_DIR}" -B "${library_build}"
    -DHASHMAP_BUILD_TESTS=OFF -DHASHMAP_BUILD_EXAMPLES=OFF
    -DHASHMAP_BUILD_BENCHMARKS=OFF -DHASHMAP_INSTALL=ON
    -DCMAKE_INSTALL_INCLUDEDIR=include -DCMAKE_INSTALL_LIBDIR=lib)
run("${CMAKE_COMMAND}" --build "${library_build}" --config Debug)
run("${CMAKE_COMMAND}" --install "${library_build}" --config Debug --prefix "${prefix}")

check_consumer(embedded "-DHASHMAP_SOURCE_DIR=${HASHMAP_SOURCE_DIR}")
check_consumer(build-tree "-DHashMap_DIR=${library_build}")
check_consumer(installed "-DCMAKE_PREFIX_PATH=${prefix}")

set(custom_build "${HASHMAP_CHECK_DIR}/custom-library")
set(custom_prefix "${HASHMAP_CHECK_DIR}/custom-prefix")
set(relocated_prefix "${HASHMAP_CHECK_DIR}/relocated-prefix")
run("${CMAKE_COMMAND}" -S "${HASHMAP_SOURCE_DIR}" -B "${custom_build}"
    -DHASHMAP_BUILD_TESTS=OFF -DHASHMAP_BUILD_EXAMPLES=OFF
    -DHASHMAP_BUILD_BENCHMARKS=OFF -DHASHMAP_INSTALL=ON
    -DCMAKE_INSTALL_INCLUDEDIR=custom/include -DCMAKE_INSTALL_LIBDIR=custom/lib)
run("${CMAKE_COMMAND}" --install "${custom_build}" --config Debug --prefix "${custom_prefix}")
file(RENAME "${custom_prefix}" "${relocated_prefix}")
check_consumer(relocated "-DHashMap_DIR=${relocated_prefix}/custom/lib/cmake/HashMap")
