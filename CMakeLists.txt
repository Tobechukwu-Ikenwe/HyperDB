cmake_minimum_required(VERSION 3.25)
project(HyperDB LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(HYPERDB_SANITIZER "" CACHE STRING "Sanitizer: address, undefined, or thread")
option(HYPERDB_COVERAGE "Enable GCC/Clang coverage instrumentation" OFF)
option(HYPERDB_FUZZ "Enable fuzz development build" OFF)
option(HYPERDB_BUILD_TESTS "Build GoogleTest suites and Python integration tests" ON)
option(HYPERDB_BUILD_BENCHMARKS "Build Google Benchmark workloads" ON)
option(HYPERDB_SINGLE_THREADED "Build the core for single-threaded access without mutexes" OFF)
option(HYPERDB_ENABLE_HYPERMALLOC "Enable the optional HyperMalloc memory backend" OFF)

if(HYPERDB_ENABLE_HYPERMALLOC)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "HyperMalloc requires a 64-bit Linux target")
    endif()
    find_package(HyperMalloc CONFIG REQUIRED)
endif()

function(hyperdb_strict_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

if(HYPERDB_SANITIZER)
    if(NOT HYPERDB_SANITIZER MATCHES "^(address|undefined|thread)$")
        message(FATAL_ERROR "Unknown sanitizer: ${HYPERDB_SANITIZER}")
    endif()
    add_compile_options(-fsanitize=${HYPERDB_SANITIZER} -fno-omit-frame-pointer)
    add_link_options(-fsanitize=${HYPERDB_SANITIZER})
endif()
if(HYPERDB_COVERAGE)
    add_compile_options(--coverage)
    add_link_options(--coverage)
endif()
if(HYPERDB_FUZZ AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Fuzz build requires Clang and compiler-provided libFuzzer")
endif()

add_library(hyperdb_core
    src/common/memory_resource.cpp
    src/common/crc32c.cpp
    src/common/logging.cpp
    src/storage/fault_file.cpp
    src/storage/page_format.cpp
    src/storage/disk_manager.cpp
    src/storage/buffer_pool.cpp
    src/parser/lexer.cpp
    src/parser/parser.cpp
    src/database/database.cpp
    src/database/TableData/schema.cpp
    src/database/Indexes/btree.cpp
    src/database/Indexes/vector_index.cpp
    src/Limit/limit.cpp
    src/OrderBy/order_by.cpp
    src/QueryExecutor/query_executor.cpp
    src/QueryExecutor/statement_executor.cpp
    src/QueryExecutor/aggregate.cpp
    src/filter.cpp
    src/filter_table.cpp
)
target_include_directories(hyperdb_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include)
hyperdb_strict_warnings(hyperdb_core)
if(HYPERDB_ENABLE_HYPERMALLOC)
    target_compile_definitions(hyperdb_core PRIVATE HYPERDB_HAS_HYPERMALLOC=1)
    target_link_libraries(hyperdb_core PRIVATE HyperMalloc::hypermalloc)
endif()
if(HYPERDB_SINGLE_THREADED)
    target_compile_definitions(hyperdb_core PUBLIC HYPERDB_SINGLE_THREADED=1)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 15)
    target_compile_options(hyperdb_core PRIVATE
        $<$<CONFIG:Release>:-Wno-free-nonheap-object>)
endif()

add_library(hyperdb_posix_file src/storage/posix_file.cpp)
target_link_libraries(hyperdb_posix_file PUBLIC hyperdb_core)
hyperdb_strict_warnings(hyperdb_posix_file)

add_library(hyperdb_snapshot_compat src/database/persistence.cpp)
target_link_libraries(hyperdb_snapshot_compat PUBLIC hyperdb_core)
hyperdb_strict_warnings(hyperdb_snapshot_compat)

add_library(hyperdb_c_api src/bridge/c_api.cpp)
target_link_libraries(hyperdb_c_api PUBLIC hyperdb_core)
hyperdb_strict_warnings(hyperdb_c_api)

add_executable(hyperdb src/main.cpp)
target_link_libraries(hyperdb PRIVATE hyperdb_snapshot_compat)

add_executable(hyperdb_vector_demo examples/vector_search.cpp)
target_link_libraries(hyperdb_vector_demo PRIVATE hyperdb_core)

add_executable(hyperdb_bridge src/bridge/sqlite_bridge.cpp)
target_link_libraries(hyperdb_bridge PRIVATE hyperdb_core)

add_executable(hyperdb_c_example examples/c_api_example.c)
target_link_libraries(hyperdb_c_example PRIVATE hyperdb_c_api)

add_executable(hyperdb_cpp_example examples/cpp_embed.cpp)
target_link_libraries(hyperdb_cpp_example PRIVATE hyperdb_core)

if(HYPERDB_FUZZ)
    target_compile_options(hyperdb_core PRIVATE -fsanitize=fuzzer-no-link)
    add_executable(hyperdb_parser_fuzz tests/fuzz/parser_fuzz.cpp)
    target_link_libraries(hyperdb_parser_fuzz PRIVATE hyperdb_core)
    target_compile_options(hyperdb_parser_fuzz PRIVATE -fsanitize=fuzzer-no-link)
    target_link_options(hyperdb_parser_fuzz PRIVATE -fsanitize=fuzzer)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/tests/fuzz/corpus/parser
         DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/corpus)
endif()

include(FetchContent)
if(HYPERDB_BUILD_TESTS)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(googletest)
endif()
if(HYPERDB_BUILD_BENCHMARKS)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.9.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(googlebenchmark)
endif()

enable_testing()
if(HYPERDB_BUILD_TESTS)
    include(GoogleTest)
    function(hyperdb_add_gtest target source library label)
        add_executable(${target} ${source} ${ARGN})
        target_link_libraries(${target} PRIVATE ${library} GTest::gtest_main)
        gtest_discover_tests(${target}
            TEST_PREFIX "${target}/"
            PROPERTIES LABELS "${label}")
    endfunction()

    hyperdb_add_gtest(hyperdb_status_tests tests/unit/status_tests.cpp hyperdb_core unit
    tests/unit/status_gtest.cpp)
    hyperdb_add_gtest(hyperdb_memory_resource_tests tests/unit/memory_resource_tests.cpp hyperdb_core unit)
    hyperdb_add_gtest(hyperdb_allocator_backend_tests tests/unit/allocator_backend_tests.cpp hyperdb_snapshot_compat unit)
    hyperdb_add_gtest(hyperdb_c_api_tests tests/integration/c_api_tests.cpp hyperdb_c_api integration)
    hyperdb_add_gtest(hyperdb_file_system_tests tests/storage/file_system_tests.cpp hyperdb_posix_file storage)
    hyperdb_add_gtest(hyperdb_page_format_tests tests/storage/page_format_tests.cpp hyperdb_core storage)
    hyperdb_add_gtest(hyperdb_disk_manager_tests tests/storage/disk_manager_tests.cpp hyperdb_posix_file storage)
    hyperdb_add_gtest(hyperdb_device_storage_tests tests/storage/device_storage_tests.cpp hyperdb_core storage)
    hyperdb_add_gtest(hyperdb_buffer_pool_tests tests/storage/buffer_pool_tests.cpp hyperdb_posix_file storage)
    hyperdb_add_gtest(hyperdb_tests tests/unit/database_tests.cpp hyperdb_core unit)
    hyperdb_add_gtest(hyperdb_lexer_tests tests/unit/lexer_tests.cpp hyperdb_core unit)
    hyperdb_add_gtest(hyperdb_parser_tests tests/unit/parser_tests.cpp hyperdb_core unit)
    hyperdb_add_gtest(hyperdb_index_tests tests/unit/index_tests.cpp hyperdb_core unit)
    hyperdb_add_gtest(hyperdb_vector_tests tests/unit/vector_tests.cpp hyperdb_core unit)
    hyperdb_add_gtest(hyperdb_persistence_tests tests/integration/persistence_tests.cpp hyperdb_snapshot_compat integration)
    hyperdb_add_gtest(hyperdb_statement_tests tests/integration/statement_tests.cpp hyperdb_snapshot_compat integration)

    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME hyperdb_bridge_tests
            COMMAND ${CMAKE_COMMAND} -E env HYPERDB_BRIDGE_BIN=$<TARGET_FILE:hyperdb_bridge>
                    ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/bridge_tests.py)
    endif()

    if(UNIX AND Python3_Interpreter_FOUND)
        add_test(NAME hyperdb_cli_concurrency_tests
            COMMAND ${CMAKE_COMMAND} -E env HYPERDB_CLI_BIN=$<TARGET_FILE:hyperdb>
                    ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/regression/cli_concurrency_tests.py)
        add_test(NAME hyperdb_sqlite_conformance_tests
            COMMAND ${CMAKE_COMMAND} -E env HYPERDB_CLI_BIN=$<TARGET_FILE:hyperdb>
                    ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/regression/sqlite_conformance.py)
    endif()

    if(TEST hyperdb_bridge_tests)
        set_tests_properties(hyperdb_bridge_tests PROPERTIES LABELS "integration")
    endif()
    if(TEST hyperdb_cli_concurrency_tests)
        set_tests_properties(hyperdb_cli_concurrency_tests PROPERTIES LABELS "regression;concurrency")
    endif()
    if(TEST hyperdb_sqlite_conformance_tests)
        set_tests_properties(hyperdb_sqlite_conformance_tests PROPERTIES LABELS "regression")
    endif()
endif()

if(HYPERDB_BUILD_BENCHMARKS)
    add_executable(hyperdb_benchmark benchmarks/workloads/equality_benchmark.cpp)
    target_link_libraries(hyperdb_benchmark PRIVATE hyperdb_core benchmark::benchmark_main)
    add_executable(hyperdb_workloads benchmarks/workloads/workloads.cpp)
    target_link_libraries(hyperdb_workloads PRIVATE hyperdb_snapshot_compat benchmark::benchmark_main)
    add_executable(hyperdb_file_io_benchmark benchmarks/workloads/file_io_benchmark.cpp)
    target_link_libraries(hyperdb_file_io_benchmark PRIVATE hyperdb_posix_file benchmark::benchmark_main)
endif()
