cmake_minimum_required(VERSION 3.25)
project(HyperDB LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(HYPERDB_SANITIZER "" CACHE STRING "Sanitizer: address, undefined, or thread")
option(HYPERDB_COVERAGE "Enable GCC/Clang coverage instrumentation" OFF)
option(HYPERDB_FUZZ "Enable fuzz development build" OFF)
option(HYPERDB_FRAMEWORKS "Build pinned GoogleTest and Google Benchmark targets" OFF)
option(HYPERDB_SINGLE_THREADED "Build the core for single-threaded access without mutexes" OFF)

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

add_executable(hyperdb_benchmark benchmarks/workloads/equality_benchmark.cpp)
target_link_libraries(hyperdb_benchmark PRIVATE hyperdb_core)

add_executable(hyperdb_workloads benchmarks/workloads/workloads.cpp)
target_link_libraries(hyperdb_workloads PRIVATE hyperdb_snapshot_compat)

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

enable_testing()
if(HYPERDB_FRAMEWORKS)
    include(FetchContent)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
        GIT_SHALLOW TRUE)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.9.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(googletest googlebenchmark)

    add_executable(hyperdb_gtest tests/unit/status_gtest.cpp)
    target_link_libraries(hyperdb_gtest PRIVATE hyperdb_core GTest::gtest_main)
    add_test(NAME hyperdb_gtest COMMAND hyperdb_gtest)

    add_executable(hyperdb_file_io_benchmark benchmarks/workloads/file_io_benchmark.cpp)
    target_link_libraries(hyperdb_file_io_benchmark PRIVATE hyperdb_posix_file benchmark::benchmark_main)
endif()
add_executable(hyperdb_status_tests tests/unit/status_tests.cpp)
target_link_libraries(hyperdb_status_tests PRIVATE hyperdb_core)
add_test(NAME hyperdb_status_tests COMMAND hyperdb_status_tests)
add_executable(hyperdb_c_api_tests tests/integration/c_api_tests.c)
target_link_libraries(hyperdb_c_api_tests PRIVATE hyperdb_c_api)
add_test(NAME hyperdb_c_api_tests COMMAND hyperdb_c_api_tests)
add_executable(hyperdb_file_system_tests tests/storage/file_system_tests.cpp)
target_link_libraries(hyperdb_file_system_tests PRIVATE hyperdb_posix_file)
add_test(NAME hyperdb_file_system_tests COMMAND hyperdb_file_system_tests)
add_executable(hyperdb_page_format_tests tests/storage/page_format_tests.cpp)
target_link_libraries(hyperdb_page_format_tests PRIVATE hyperdb_core)
add_test(NAME hyperdb_page_format_tests COMMAND hyperdb_page_format_tests)
add_executable(hyperdb_disk_manager_tests tests/storage/disk_manager_tests.cpp)
target_link_libraries(hyperdb_disk_manager_tests PRIVATE hyperdb_posix_file)
add_test(NAME hyperdb_disk_manager_tests COMMAND hyperdb_disk_manager_tests)
add_executable(hyperdb_device_storage_tests tests/storage/device_storage_tests.cpp)
target_link_libraries(hyperdb_device_storage_tests PRIVATE hyperdb_core)
add_test(NAME hyperdb_device_storage_tests COMMAND hyperdb_device_storage_tests)
add_executable(hyperdb_buffer_pool_tests tests/storage/buffer_pool_tests.cpp)
target_link_libraries(hyperdb_buffer_pool_tests PRIVATE hyperdb_posix_file)
add_test(NAME hyperdb_buffer_pool_tests COMMAND hyperdb_buffer_pool_tests)
add_executable(hyperdb_tests tests/unit/database_tests.cpp)
target_link_libraries(hyperdb_tests PRIVATE hyperdb_core)
add_test(NAME hyperdb_tests COMMAND hyperdb_tests)

add_executable(hyperdb_lexer_tests tests/unit/lexer_tests.cpp)
target_link_libraries(hyperdb_lexer_tests PRIVATE hyperdb_core)
add_test(NAME hyperdb_lexer_tests COMMAND hyperdb_lexer_tests)

add_executable(hyperdb_parser_tests tests/unit/parser_tests.cpp)
target_link_libraries(hyperdb_parser_tests PRIVATE hyperdb_core)
add_test(NAME hyperdb_parser_tests COMMAND hyperdb_parser_tests)

add_executable(hyperdb_index_tests tests/unit/index_tests.cpp)
target_link_libraries(hyperdb_index_tests PRIVATE hyperdb_core)
add_test(NAME hyperdb_index_tests COMMAND hyperdb_index_tests)

add_executable(hyperdb_vector_tests tests/unit/vector_tests.cpp)
target_link_libraries(hyperdb_vector_tests PRIVATE hyperdb_core)
add_test(NAME hyperdb_vector_tests COMMAND hyperdb_vector_tests)

add_executable(hyperdb_persistence_tests tests/integration/persistence_tests.cpp)
target_link_libraries(hyperdb_persistence_tests PRIVATE hyperdb_snapshot_compat)
add_test(NAME hyperdb_persistence_tests COMMAND hyperdb_persistence_tests)

add_executable(hyperdb_statement_tests tests/integration/statement_tests.cpp)
target_link_libraries(hyperdb_statement_tests PRIVATE hyperdb_snapshot_compat)
add_test(NAME hyperdb_statement_tests COMMAND hyperdb_statement_tests)

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

set_tests_properties(
    hyperdb_status_tests hyperdb_tests hyperdb_lexer_tests hyperdb_parser_tests
    hyperdb_index_tests hyperdb_vector_tests
    PROPERTIES LABELS "unit")
set_tests_properties(
    hyperdb_file_system_tests hyperdb_page_format_tests hyperdb_disk_manager_tests
    hyperdb_device_storage_tests hyperdb_buffer_pool_tests
    PROPERTIES LABELS "storage")
set_tests_properties(
    hyperdb_c_api_tests hyperdb_persistence_tests hyperdb_statement_tests
    PROPERTIES LABELS "integration")
if(TEST hyperdb_bridge_tests)
    set_tests_properties(hyperdb_bridge_tests PROPERTIES LABELS "integration")
endif()
if(TEST hyperdb_cli_concurrency_tests)
    set_tests_properties(hyperdb_cli_concurrency_tests PROPERTIES LABELS "regression;concurrency")
endif()
if(TEST hyperdb_sqlite_conformance_tests)
    set_tests_properties(hyperdb_sqlite_conformance_tests PROPERTIES LABELS "regression")
endif()
