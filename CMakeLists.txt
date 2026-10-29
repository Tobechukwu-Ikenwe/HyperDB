cmake_minimum_required(VERSION 3.25)
project(HyperDB LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(HYPERDB_SANITIZER "" CACHE STRING "Sanitizer: address, undefined, or thread")
option(HYPERDB_COVERAGE "Enable GCC/Clang coverage instrumentation" OFF)
option(HYPERDB_FUZZ "Enable fuzz development build" OFF)
option(HYPERDB_FRAMEWORKS "Build pinned GoogleTest and Google Benchmark targets" OFF)
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
    src/storage/posix_file.cpp
    src/storage/fault_file.cpp
    src/storage/page_format.cpp
    src/storage/disk_manager.cpp
    src/parser/lexer.cpp
    src/parser/parser.cpp
    src/database/database.cpp
    src/database/persistence.cpp
    src/database/TableData/schema.cpp
    src/database/Indexes/btree.cpp
    src/database/Indexes/vector_index.cpp
    src/from/from.cpp
    src/Limit/limit.cpp
    src/OrderBy/order_by.cpp
    src/QueryExecutor/query_executor.cpp
    src/QueryExecutor/statement_executor.cpp
    src/QueryExecutor/aggregate.cpp
    src/filter.cpp
    src/filter_table.cpp
    src/select.cpp
)
target_include_directories(hyperdb_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_source_files_properties(
    src/common/logging.cpp
    src/common/crc32c.cpp
    src/storage/posix_file.cpp
    src/storage/fault_file.cpp
    src/storage/page_format.cpp
    src/storage/disk_manager.cpp
    PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Wpedantic;-Werror"
)

add_executable(hyperdb src/main.cpp)
target_link_libraries(hyperdb PRIVATE hyperdb_core)

add_executable(hyperdb_benchmark benchmarks/benchmark.cpp)
target_link_libraries(hyperdb_benchmark PRIVATE hyperdb_core)

add_executable(hyperdb_workloads benchmarks/workloads.cpp)
target_link_libraries(hyperdb_workloads PRIVATE hyperdb_core)

add_executable(hyperdb_vector_demo examples/vector_search.cpp)
target_link_libraries(hyperdb_vector_demo PRIVATE hyperdb_core)

add_executable(hyperdb_bridge src/bridge/sqlite_bridge.cpp)
target_link_libraries(hyperdb_bridge PRIVATE hyperdb_core)

if(HYPERDB_FUZZ)
    target_compile_options(hyperdb_core PRIVATE -fsanitize=fuzzer-no-link)
    add_executable(hyperdb_parser_fuzz fuzz/parser_fuzz.cpp)
    target_link_libraries(hyperdb_parser_fuzz PRIVATE hyperdb_core)
    target_compile_options(hyperdb_parser_fuzz PRIVATE -fsanitize=fuzzer-no-link)
    target_link_options(hyperdb_parser_fuzz PRIVATE -fsanitize=fuzzer)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parser
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

    add_executable(hyperdb_file_io_benchmark benchmarks/file_io_benchmark.cpp)
    target_link_libraries(hyperdb_file_io_benchmark PRIVATE hyperdb_core benchmark::benchmark_main)
endif()
add_executable(hyperdb_status_tests tests/status_tests.cpp)
target_link_libraries(hyperdb_status_tests PRIVATE hyperdb_core)
add_test(NAME hyperdb_status_tests COMMAND hyperdb_status_tests)
add_executable(hyperdb_file_system_tests tests/file_system_tests.cpp)
target_link_libraries(hyperdb_file_system_tests PRIVATE hyperdb_core)
add_test(NAME hyperdb_file_system_tests COMMAND hyperdb_file_system_tests)
add_executable(hyperdb_page_format_tests tests/page_format_tests.cpp)
target_link_libraries(hyperdb_page_format_tests PRIVATE hyperdb_core)
add_test(NAME hyperdb_page_format_tests COMMAND hyperdb_page_format_tests)
add_executable(hyperdb_disk_manager_tests tests/disk_manager_tests.cpp)
target_link_libraries(hyperdb_disk_manager_tests PRIVATE hyperdb_core)
add_test(NAME hyperdb_disk_manager_tests COMMAND hyperdb_disk_manager_tests)
add_executable(hyperdb_tests tests/database_tests.cpp)
target_link_libraries(hyperdb_tests PRIVATE hyperdb_core)
add_test(NAME hyperdb_tests COMMAND hyperdb_tests)

add_executable(hyperdb_lexer_tests tests/lexer_tests.cpp)
target_link_libraries(hyperdb_lexer_tests PRIVATE hyperdb_core)
add_test(NAME hyperdb_lexer_tests COMMAND hyperdb_lexer_tests)

add_executable(hyperdb_parser_tests tests/parser_tests.cpp)
target_link_libraries(hyperdb_parser_tests PRIVATE hyperdb_core)
add_test(NAME hyperdb_parser_tests COMMAND hyperdb_parser_tests)

add_executable(hyperdb_index_tests tests/index_tests.cpp)
target_link_libraries(hyperdb_index_tests PRIVATE hyperdb_core)
add_test(NAME hyperdb_index_tests COMMAND hyperdb_index_tests)

add_executable(hyperdb_vector_tests tests/vector_tests.cpp)
target_link_libraries(hyperdb_vector_tests PRIVATE hyperdb_core)
add_test(NAME hyperdb_vector_tests COMMAND hyperdb_vector_tests)

add_executable(hyperdb_persistence_tests tests/persistence_tests.cpp)
target_link_libraries(hyperdb_persistence_tests PRIVATE hyperdb_core)
add_test(NAME hyperdb_persistence_tests COMMAND hyperdb_persistence_tests)

add_executable(hyperdb_statement_tests tests/statement_tests.cpp)
target_link_libraries(hyperdb_statement_tests PRIVATE hyperdb_core)
add_test(NAME hyperdb_statement_tests COMMAND hyperdb_statement_tests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME hyperdb_bridge_tests
        COMMAND ${CMAKE_COMMAND} -E env HYPERDB_BRIDGE_BIN=$<TARGET_FILE:hyperdb_bridge>
                ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/bridge_tests.py)
endif()

if(UNIX AND Python3_Interpreter_FOUND)
    add_test(NAME hyperdb_cli_concurrency_tests
        COMMAND ${CMAKE_COMMAND} -E env HYPERDB_CLI_BIN=$<TARGET_FILE:hyperdb>
                ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_concurrency_tests.py)
    add_test(NAME hyperdb_sqlite_conformance_tests
        COMMAND ${CMAKE_COMMAND} -E env HYPERDB_CLI_BIN=$<TARGET_FILE:hyperdb>
                ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/sqlite_conformance.py)
endif()
