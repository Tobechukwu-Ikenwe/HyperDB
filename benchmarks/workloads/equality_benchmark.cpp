#include "QueryExecutor/query_executor.h"

#include <benchmark/benchmark.h>

#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

Database make_database(std::size_t row_count, bool with_index) {
    Table table;
    table.schema.add_column({"id", "INTEGER", "INTEGER", false});
    table.schema.add_column({"payload", "TEXT", "TEXT", false});
    table.rows.reserve(row_count);
    for (std::size_t i = 0; i < row_count; ++i) {
        table.rows.push_back({static_cast<std::int64_t>(i), "item" + std::to_string(i)});
    }

    Database database;
    database.create_table("Items", table);
    if (with_index && !database.create_index("Items", "id")) {
        throw std::runtime_error("Failed to create equality benchmark index");
    }
    return database;
}

void equality_lookup(benchmark::State &state, bool with_index) {
    const auto row_count = static_cast<std::size_t>(state.range(0));
    auto database = make_database(row_count, with_index);
    const auto expected = "item" + std::to_string(row_count - 1);
    const auto query = "SELECT payload FROM Items WHERE id = " + std::to_string(row_count - 1);
    for (auto _ : state) {
        (void)_;
        const auto result = execute_query(database, query);
        if (result.rows.size() != 1 || std::get<std::string>(result.rows[0][0]) != expected) {
            state.SkipWithError("Equality lookup returned an unexpected result");
            break;
        }
        benchmark::DoNotOptimize(result.rows.data());
    }
    state.SetItemsProcessed(state.iterations());
}

void BM_EqualityScan(benchmark::State &state) { equality_lookup(state, false); }
void BM_EqualityIndexed(benchmark::State &state) { equality_lookup(state, true); }

BENCHMARK(BM_EqualityScan)->Arg(100000)->Arg(250000);
BENCHMARK(BM_EqualityIndexed)->Arg(100000)->Arg(250000);

} // namespace
