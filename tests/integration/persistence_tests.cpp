#include "database/database.h"
#include <gtest/gtest.h>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

TEST(PersistenceTests, ExistingCases) {
    const auto path = std::filesystem::temp_directory_path() /
                      ("hyperdb-snapshot-test-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        Table table;
        table.schema.add_column({"id", "INTEGER", "INTEGER", false, true});
        table.schema.add_column({"title", "TEXT", "TEXT", false});
        table.schema.add_column({"score", "REAL", "REAL", true, false, false, 1.5});
        table.schema.add_column({"bytes", "BLOB", "BLOB"});
        table.rows = {
            {std::int64_t{1}, std::string{"Alpha"}, 2.25, std::vector<std::uint8_t>{0, 255}},
            {std::int64_t{2}, std::string{"Beta"}, std::monostate{}, std::vector<std::uint8_t>{}}};
        Database original;
        EXPECT_TRUE(original.create_table("Documents", table));
        EXPECT_TRUE(original.create_index("Documents", "id"));
        EXPECT_TRUE(original.create_vector_index("Documents", 2));
        EXPECT_TRUE(original.set_embedding("Documents", 0, {1, 0}));
        EXPECT_TRUE(original.set_embedding("Documents", 1, {0, 1}));
        original.save(path);

        auto loaded = Database::load(path);
        EXPECT_TRUE(loaded.table_count() == 1);
        const auto *restored = loaded.find_table("documents");
        EXPECT_TRUE(restored && restored->rows == table.rows);
        const auto &columns = restored->schema.get_columns();
        EXPECT_TRUE(columns[0].primary_key && !columns[0].nullable);
        EXPECT_TRUE(columns[2].declared_type == "REAL");
        EXPECT_TRUE(columns[2].default_value == Value{1.5});
        EXPECT_TRUE(loaded.lookup_index("DOCUMENTS", "ID", std::int64_t{2}).value() ==
                    std::vector<RowID>{1});
        auto matches = loaded.vector_search("documents", {1, 0}, 2);
        EXPECT_TRUE(matches.size() == 2 && matches[0].row_id == 0 && matches[1].row_id == 1);
        EXPECT_TRUE(loaded.insert_row("Documents", {std::int64_t{3}, std::string{"Gamma"}, 3.5,
                                                    std::vector<std::uint8_t>{1}}));
        EXPECT_TRUE(loaded.lookup_index("documents", "id", std::int64_t{3}).value() ==
                    std::vector<RowID>{2});
        loaded.save(path);
        EXPECT_TRUE(Database::load(path).find_table("Documents")->rows.size() == 3);

        std::ofstream corrupt(path, std::ios::binary | std::ios::trunc);
        corrupt << "HDBSNAP1";
        corrupt.close();
        bool rejected = false;
        try {
            (void)Database::load(path);
        } catch (const std::exception &) {
            rejected = true;
        }
        EXPECT_TRUE(rejected);
        std::filesystem::remove(path);
    } catch (...) {
        std::filesystem::remove(path);
        throw;
    }
    std::cout << "Persistence tests passed.\n";
}
