#include "QueryExecutor/query_executor.h"
#include "database/Indexes/btree.h"
#include "database/database.h"
#include <gtest/gtest.h>

#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>

TEST(IndexTests, ExistingCases) {
    BTree tree(2);
    EXPECT_TRUE(tree.empty());
    for (std::int64_t i = 0; i < 997; ++i)
        tree.insert((i * 37) % 997, static_cast<RowID>(i));
    EXPECT_TRUE(tree.size() == 997);
    for (std::int64_t i = 0; i < 997; ++i) {
        auto found = tree.search((i * 37) % 997);
        EXPECT_TRUE(found && found->size() == 1 && found->front() == static_cast<RowID>(i));
    }
    tree.insert(42.0, 1000);
    auto duplicate = tree.search(std::int64_t{42});
    EXPECT_TRUE(duplicate && duplicate->size() == 2 && duplicate->back() == 1000);
    EXPECT_TRUE(!tree.search(std::int64_t{1001}));
    try {
        BTree invalid(1);
        EXPECT_TRUE(false);
    } catch (const std::invalid_argument &) {
    }

    Table table;
    table.schema.add_column({"id", "INTEGER", "INTEGER", false});
    table.schema.add_column({"name", "TEXT", "TEXT"});
    for (std::int64_t i = 0; i < 100; ++i)
        table.rows.push_back({i, std::string{"item"} + std::to_string(i)});
    Database db;
    EXPECT_TRUE(db.create_table("Items", table));
    const auto sql = "SELECT name FROM Items WHERE id = 42";
    const auto before = execute_query(db, sql);
    EXPECT_TRUE(db.create_index("Items", "id"));
    EXPECT_TRUE(!db.create_index("Items", "id"));
    const auto after = execute_query(db, sql);
    EXPECT_TRUE(after.rows == before.rows && after.columns == before.columns);
    EXPECT_TRUE(std::get<std::string>(after.rows[0][0]) == "item42");
    EXPECT_TRUE(db.lookup_index("Items", "id", std::int64_t{42})->front() == 42);
    EXPECT_TRUE(db.insert_row("Items", {std::int64_t{42}, std::string{"duplicate"}}));
    auto matches = execute_query(db, sql);
    EXPECT_TRUE(matches.rows.size() == 2);
    EXPECT_TRUE(std::get<std::string>(matches.rows[1][0]) == "duplicate");
    EXPECT_TRUE(db.lookup_index("Items", "id", std::int64_t{42})->size() == 2);
    auto and_result =
        execute_query(db, "SELECT name FROM Items WHERE id = 42 AND name = 'duplicate'");
    EXPECT_TRUE(and_result.rows.size() == 1);
    auto reverse_result = execute_query(db, "SELECT name FROM Items WHERE 42 = id");
    EXPECT_TRUE(reverse_result.rows.size() == 2);
    try {
        (void)db.insert_row("Items", {std::string{"bad"}, std::string{"bad"}});
        EXPECT_TRUE(false);
    } catch (const std::invalid_argument &) {
    }
    EXPECT_TRUE(db.find_table("Items")->rows.size() == 101);
    EXPECT_TRUE(db.drop_table("Items"));
    EXPECT_TRUE(!db.lookup_index("Items", "id", std::int64_t{42}));
    std::cout << "Index tests passed.\n";
}
