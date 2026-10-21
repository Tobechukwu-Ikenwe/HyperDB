#include "database/database.h"
#include "QueryExecutor/query_executor.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    const auto row_count = argc > 1 ? std::stoull(argv[1]) : 100000ULL;
    const auto iterations = argc > 2 ? std::stoull(argv[2]) : 200ULL;
    if (row_count == 0 || iterations == 0) throw std::invalid_argument("Rows and iterations must be positive");
    Table table;
    table.schema.add_column({"id", "INTEGER", "INTEGER", false});
    table.schema.add_column({"payload", "TEXT", "TEXT", false});
    table.rows.reserve(row_count);
    for (std::uint64_t i = 0; i < row_count; ++i)
        table.rows.push_back({static_cast<std::int64_t>(i), std::string{"item"} + std::to_string(i)});
    Database database;
    database.create_table("Items", table);
    const std::string sql = "SELECT payload FROM Items WHERE id = " + std::to_string(row_count - 1);
    auto measure = [&]() {
        const auto start = std::chrono::steady_clock::now();
        for (std::uint64_t i = 0; i < iterations; ++i) {
            const auto result = execute_query(database, sql);
            if (result.rows.size() != 1 ||
                std::get<std::string>(result.rows[0][0]) != "item" + std::to_string(row_count - 1))
                throw std::runtime_error("Benchmark result mismatch");
        }
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    };
    const auto scan_ms = measure();
    database.create_index("Items", "id");
    const auto index_ms = measure();
    std::cout << "rows=" << row_count << " iterations=" << iterations << '\n';
    std::cout << "scan_ms=" << scan_ms << " index_ms=" << index_ms << '\n';
}
