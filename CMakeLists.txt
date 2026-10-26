cmake_minimum_required(VERSION 3.16)
project(HyperDB LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(hyperdb_core
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
target_include_directories(hyperdb_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

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

enable_testing()
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
