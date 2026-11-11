#include "QueryExecutor/query_executor.h"
#include "hyperdb/memory.h"
#include <gtest/gtest.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>

namespace {
struct Ledger {
    std::size_t allocations = 0;
    std::size_t deallocations = 0;
};

std::size_t actual_alignment(std::size_t alignment) {
    return alignment < alignof(std::max_align_t) ? alignof(std::max_align_t) : alignment;
}

void *allocate(void *context, std::size_t bytes, std::size_t alignment) noexcept {
    auto &ledger = *static_cast<Ledger *>(context);
    void *pointer =
        alignment <= alignof(std::max_align_t)
            ? ::operator new(bytes == 0 ? 1 : bytes, std::nothrow)
            : ::operator new(bytes == 0 ? 1 : bytes, std::align_val_t{actual_alignment(alignment)},
                             std::nothrow);
    if (pointer)
        ++ledger.allocations;
    return pointer;
}

void deallocate(void *context, void *pointer, std::size_t, std::size_t alignment) noexcept {
    if (!pointer)
        return;
    auto &ledger = *static_cast<Ledger *>(context);
    ++ledger.deallocations;
    if (alignment <= alignof(std::max_align_t))
        ::operator delete(pointer);
    else
        ::operator delete(pointer, std::align_val_t{actual_alignment(alignment)});
}

void verify_sql_workload(hyperdb::AllocatorConfig config, Ledger *ledger) {
    {
        Database database{config};
        auto created =
            execute_statement(database, "CREATE TABLE people (id INTEGER NOT NULL, name TEXT)");
        EXPECT_TRUE(created.modified);
        EXPECT_TRUE(database.allocator_stats().allocation_count > 0);
        EXPECT_TRUE(execute_statement(database, "INSERT INTO people VALUES (1, 'Ada')").modified);
        EXPECT_TRUE(execute_statement(database, "INSERT INTO people VALUES (2, 'Grace')").modified);
        const auto before_index = database.allocator_stats().allocation_count;
        EXPECT_TRUE(database.create_index("people", "id"));
        EXPECT_TRUE(database.allocator_stats().allocation_count > before_index);
        auto selected = execute_statement(database, "SELECT name FROM people WHERE id = 2");
        EXPECT_TRUE(selected.query.rows.size() == 1);
        EXPECT_TRUE(std::get<std::string>(selected.query.rows[0][0]) == "Grace");
        auto stats = database.allocator_stats();
        EXPECT_TRUE(stats.current_bytes > 0);
        EXPECT_TRUE(stats.allocation_count >= 2);
        EXPECT_TRUE(stats.failure_count == 0);
    }
    if (ledger) {
        EXPECT_TRUE(ledger->allocations > 0);
        EXPECT_TRUE(ledger->allocations == ledger->deallocations);
    }
}
} // namespace

TEST(AllocatorBackendTests, ExistingCases) {
    verify_sql_workload({}, nullptr);

    Ledger application_ledger;
    hyperdb::AllocatorConfig application;
    application.backend = hyperdb::AllocatorBackend::application;
    application.context = &application_ledger;
    application.allocate = allocate;
    application.deallocate = deallocate;
    verify_sql_workload(application, &application_ledger);
    {
        Database first{application};
        Database second{application};
        EXPECT_TRUE(execute_statement(first, "CREATE TABLE first_table (id INTEGER)").modified);
        EXPECT_TRUE(execute_statement(second, "CREATE TABLE second_table (id INTEGER)").modified);
        EXPECT_TRUE(first.allocator_stats().current_bytes > 0);
        EXPECT_TRUE(second.allocator_stats().current_bytes > 0);
    }
    EXPECT_TRUE(application_ledger.allocations == application_ledger.deallocations);

    if (hyperdb::MemoryResource::hypermalloc_available()) {
        hyperdb::AllocatorConfig hypermalloc;
        hypermalloc.backend = hyperdb::AllocatorBackend::hypermalloc;
        verify_sql_workload(hypermalloc, nullptr);
    }

    hyperdb::AllocatorConfig exhausted;
    exhausted.budget_bytes = 1;
    Database database{exhausted};
    bool failed = false;
    try {
        (void)execute_statement(database, "CREATE TABLE blocked (id INTEGER NOT NULL)");
    } catch (const std::bad_alloc &) {
        failed = true;
    }
    EXPECT_TRUE(failed);
    EXPECT_TRUE(database.table_count() == 0);
    const auto failure_stats = database.allocator_stats();
    EXPECT_TRUE(failure_stats.current_bytes == 0);
    EXPECT_TRUE(failure_stats.failure_count == 1);
}
