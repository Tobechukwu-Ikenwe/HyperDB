#include "QueryExecutor/query_executor.h"
#include "parser/parser.h"

#include <benchmark/benchmark.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr std::size_t kRows = 20000;

Database make_database(std::size_t count) {
    Table table;
    table.schema.add_column({"id", "INTEGER", "INTEGER", false});
    table.schema.add_column({"category", "INTEGER", "INTEGER", false});
    table.schema.add_column({"payload", "TEXT", "TEXT", false});
    table.rows.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        table.rows.push_back({static_cast<std::int64_t>(i), static_cast<std::int64_t>(i % 10),
                              "item" + std::to_string(i)});
    }
    Database database;
    database.create_table("Items", table);
    return database;
}

void BM_ParseSelect(benchmark::State &state) {
    constexpr auto *sql =
        "SELECT payload FROM Items WHERE id >= 10 AND category = 2 ORDER BY id LIMIT 10";
    for (auto _ : state) {
        (void)_;
        const auto statement = parse_statement(sql);
        if (!std::holds_alternative<SelectStatement>(statement)) {
            state.SkipWithError("SELECT parser returned an unexpected statement");
            break;
        }
        benchmark::DoNotOptimize(&statement);
    }
}

template <typename Operation> void with_database(benchmark::State &state, Operation &&operation) {
    auto database = make_database(kRows);
    for (auto _ : state) {
        (void)_;
        const auto result = operation(database);
        if (!result) {
            state.SkipWithError("Query workload returned an unexpected result");
            break;
        }
    }
    state.SetItemsProcessed(state.iterations());
}

void BM_ScanEquality(benchmark::State &state) {
    with_database(state, [](Database &database) {
        const auto result = execute_query(database, "SELECT payload FROM Items WHERE id = 19999");
        return result.rows.size() == 1;
    });
}

void BM_ScanLimit(benchmark::State &state) {
    with_database(state, [](Database &database) {
        const auto result =
            execute_query(database, "SELECT payload FROM Items WHERE id >= 0 LIMIT 10");
        return result.rows.size() == 10;
    });
}

void BM_SortLimit(benchmark::State &state) {
    with_database(state, [](Database &database) {
        const auto result =
            execute_query(database, "SELECT payload FROM Items ORDER BY id DESC LIMIT 10");
        return result.rows.size() == 10 &&
               std::get<std::string>(result.rows.front().front()) == "item19999";
    });
}

void BM_Aggregate(benchmark::State &state) {
    with_database(state, [](Database &database) {
        const auto result =
            execute_query(database, "SELECT category, COUNT(*) FROM Items GROUP BY category");
        return result.rows.size() == 10;
    });
}

void BM_IndexBuild(benchmark::State &state) {
    for (auto _ : state) {
        (void)_;
        state.PauseTiming();
        bool created = false;
        {
            auto database = make_database(kRows);
            state.ResumeTiming();
            created = database.create_index("Items", "id");
            state.PauseTiming();
        }
        state.ResumeTiming();
        if (!created) {
            state.SkipWithError("Equality index construction failed");
            break;
        }
    }
}

void BM_IndexedEquality(benchmark::State &state) {
    auto database = make_database(kRows);
    if (!database.create_index("Items", "id")) {
        state.SkipWithError("Equality index construction failed");
        return;
    }
    for (auto _ : state) {
        (void)_;
        const auto result = execute_query(database, "SELECT payload FROM Items WHERE id = 19999");
        if (result.rows.size() != 1) {
            state.SkipWithError("Indexed lookup returned an unexpected result");
            break;
        }
        benchmark::DoNotOptimize(result.rows.data());
    }
    state.SetItemsProcessed(state.iterations());
}

Database make_join_database() {
    Table left;
    Table right;
    left.schema.add_column({"id", "INTEGER", "INTEGER", false});
    right.schema.add_column({"id", "INTEGER", "INTEGER", false});
    for (std::int64_t i = 0; i < 150; ++i) {
        left.rows.push_back({i});
        right.rows.push_back({i});
    }
    Database database;
    database.create_table("LeftSide", left);
    database.create_table("RightSide", right);
    return database;
}

void BM_InnerJoin(benchmark::State &state) {
    auto database = make_join_database();
    for (auto _ : state) {
        (void)_;
        const auto result = execute_query(
            database,
            "SELECT LeftSide.id FROM LeftSide JOIN RightSide ON LeftSide.id = RightSide.id");
        if (result.rows.size() != 150) {
            state.SkipWithError("Join returned an unexpected result");
            break;
        }
        benchmark::DoNotOptimize(result.rows.data());
    }
}

Database make_vector_database() {
    auto database = make_database(2000);
    database.create_vector_index("Items", 8);
    for (std::size_t i = 0; i < 2000; ++i) {
        std::vector<double> embedding(8, 0.1);
        embedding[i % 8] = 1.0;
        database.set_embedding("Items", i, std::move(embedding));
    }
    return database;
}

void BM_VectorSearch(benchmark::State &state) {
    auto database = make_vector_database();
    for (auto _ : state) {
        (void)_;
        const auto result = database.vector_search("Items", {1, 0, 0, 0, 0, 0, 0, 0}, 10);
        if (result.size() != 10) {
            state.SkipWithError("Vector search returned an unexpected result");
            break;
        }
        benchmark::DoNotOptimize(result.data());
    }
}

void BM_InsertRow(benchmark::State &state) {
    auto database = make_database(5000);
    for (auto _ : state) {
        (void)_;
        const auto id = static_cast<std::int64_t>(database.find_table("Items")->rows.size());
        if (!database.insert_row("Items", {id, id % 10, std::string{"new"}})) {
            state.SkipWithError("Row insertion failed");
            break;
        }
    }
    state.SetItemsProcessed(state.iterations());
}

void BM_UpdateOne(benchmark::State &state) {
    auto database = make_database(5000);
    for (auto _ : state) {
        (void)_;
        const auto result =
            execute_statement(database, "UPDATE Items SET payload = 'changed' WHERE id = 4999");
        if (result.rows_affected != 1) {
            state.SkipWithError("UPDATE affected an unexpected number of rows");
            break;
        }
    }
}

void BM_DeleteMissing(benchmark::State &state) {
    auto database = make_database(5000);
    for (auto _ : state) {
        (void)_;
        const auto result = execute_statement(database, "DELETE FROM Items WHERE id = -1");
        if (result.rows_affected != 0) {
            state.SkipWithError("DELETE unexpectedly removed a row");
            break;
        }
    }
}

std::filesystem::path snapshot_path() {
    return std::filesystem::temp_directory_path() /
           ("hyperdb-workload-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".hdb");
}

void BM_SnapshotSave(benchmark::State &state) {
    auto database = make_database(5000);
    const auto path = snapshot_path();
    for (auto _ : state) {
        (void)_;
        database.save(path);
    }
    std::filesystem::remove(path);
}

void BM_SnapshotLoad(benchmark::State &state) {
    auto database = make_database(5000);
    const auto path = snapshot_path();
    database.save(path);
    for (auto _ : state) {
        (void)_;
        auto restored = Database::load(path);
        if (restored.find_table("Items")->rows.size() != 5000) {
            state.SkipWithError("Snapshot load returned an unexpected row count");
            break;
        }
        benchmark::DoNotOptimize(restored.table_count());
    }
    std::filesystem::remove(path);
}

BENCHMARK(BM_ParseSelect);
BENCHMARK(BM_ScanEquality);
BENCHMARK(BM_ScanLimit);
BENCHMARK(BM_SortLimit);
BENCHMARK(BM_Aggregate);
BENCHMARK(BM_IndexBuild)->Iterations(1);
BENCHMARK(BM_IndexedEquality);
BENCHMARK(BM_InnerJoin);
BENCHMARK(BM_VectorSearch);
BENCHMARK(BM_InsertRow);
BENCHMARK(BM_UpdateOne)->Iterations(5);
BENCHMARK(BM_DeleteMissing)->Iterations(5);
BENCHMARK(BM_SnapshotSave)->Iterations(3);
BENCHMARK(BM_SnapshotLoad)->Iterations(3);

} // namespace
