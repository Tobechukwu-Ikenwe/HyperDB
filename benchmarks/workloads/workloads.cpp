#include "QueryExecutor/query_executor.h"
#include "parser/parser.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
template <class F>
double measure(const std::string& name, std::size_t iterations, F&& run) {
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < iterations; ++i) run();
    const auto milliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    std::cout << name << "\t" << iterations << "\t" << milliseconds / iterations << '\n';
    return milliseconds;
}

Database make_database(std::size_t count) {
    Table table;
    table.schema.add_column({"id", "INTEGER", "INTEGER", false});
    table.schema.add_column({"category", "INTEGER", "INTEGER", false});
    table.schema.add_column({"payload", "TEXT", "TEXT", false});
    table.rows.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
        table.rows.push_back({static_cast<std::int64_t>(i),
                              static_cast<std::int64_t>(i % 10),
                              std::string{"item"} + std::to_string(i)});
    Database database;
    database.create_table("Items", table);
    return database;
}
} // namespace

int main() {
    constexpr std::size_t rows = 20000;
    auto database = make_database(rows);
    std::cout << "workload\titerations\tms_per_operation\n";

    measure("parse_select", 1000, [&] {
        auto statement = parse_statement(
            "SELECT payload FROM Items WHERE id >= 10 AND category = 2 ORDER BY id LIMIT 10");
        if (!std::holds_alternative<SelectStatement>(statement))
            throw std::runtime_error("parse result mismatch");
    });

    measure("scan_equality", 30, [&] {
        const auto result = execute_query(database, "SELECT payload FROM Items WHERE id = 19999");
        if (result.rows.size() != 1) throw std::runtime_error("scan result mismatch");
    });
    measure("scan_limit", 30, [&] {
        const auto result = execute_query(database, "SELECT payload FROM Items WHERE id >= 0 LIMIT 10");
        if (result.rows.size() != 10) throw std::runtime_error("limit result mismatch");
    });
    measure("sort_limit", 12, [&] {
        const auto result = execute_query(database, "SELECT payload FROM Items ORDER BY id DESC LIMIT 10");
        if (result.rows.size() != 10 ||
            std::get<std::string>(result.rows.front().front()) != "item19999")
            throw std::runtime_error("sort result mismatch");
    });
    measure("aggregate", 20, [&] {
        const auto result = execute_query(database,
            "SELECT category, COUNT(*) FROM Items GROUP BY category");
        if (result.rows.size() != 10) throw std::runtime_error("aggregate result mismatch");
    });
    measure("index_build", 1, [&] {
        if (!database.create_index("Items", "id")) throw std::runtime_error("index build failed");
    });
    measure("indexed_equality", 100, [&] {
        const auto result = execute_query(database, "SELECT payload FROM Items WHERE id = 19999");
        if (result.rows.size() != 1) throw std::runtime_error("index result mismatch");
    });

    Database joined;
    Table left, right;
    left.schema.add_column({"id", "INTEGER", "INTEGER", false});
    right.schema.add_column({"id", "INTEGER", "INTEGER", false});
    for (std::int64_t i = 0; i < 150; ++i) {
        left.rows.push_back({i});
        right.rows.push_back({i});
    }
    joined.create_table("LeftSide", left);
    joined.create_table("RightSide", right);
    measure("inner_join", 15, [&] {
        const auto result = execute_query(joined,
            "SELECT LeftSide.id FROM LeftSide JOIN RightSide ON LeftSide.id = RightSide.id");
        if (result.rows.size() != 150) throw std::runtime_error("join result mismatch");
    });

    Database vectors = make_database(2000);
    vectors.create_vector_index("Items", 8);
    for (std::size_t i = 0; i < 2000; ++i) {
        std::vector<double> embedding(8, 0.1);
        embedding[i % 8] = 1.0;
        vectors.set_embedding("Items", i, std::move(embedding));
    }
    measure("vector_search", 15, [&] {
        const auto result = vectors.vector_search("Items", {1, 0, 0, 0, 0, 0, 0, 0}, 10);
        if (result.size() != 10) throw std::runtime_error("vector result mismatch");
    });

    auto writes = make_database(5000);
    measure("insert_row", 100, [&] {
        const auto id = static_cast<std::int64_t>(writes.find_table("Items")->rows.size());
        if (!writes.insert_row("Items", {id, id % 10, std::string{"new"}}))
            throw std::runtime_error("insert failed");
    });
    measure("update_one", 5, [&] {
        const auto result = execute_statement(writes,
            "UPDATE Items SET payload = 'changed' WHERE id = 4999");
        if (result.rows_affected != 1) throw std::runtime_error("update result mismatch");
    });
    measure("delete_missing", 5, [&] {
        const auto result = execute_statement(writes,
            "DELETE FROM Items WHERE id = -1");
        if (result.rows_affected != 0) throw std::runtime_error("delete result mismatch");
    });

    const auto path = std::filesystem::temp_directory_path() /
        ("hyperdb-workload-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) + ".hdb");
    try {
        measure("snapshot_save", 3, [&] { writes.save(path); });
        measure("snapshot_load", 3, [&] {
            auto restored = Database::load(path);
            if (restored.find_table("Items")->rows.size() != 5100)
                throw std::runtime_error("load result mismatch");
        });
        std::filesystem::remove(path);
    } catch (...) {
        std::filesystem::remove(path);
        throw;
    }
}
