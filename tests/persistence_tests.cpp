#include "database/database.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main() {
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
            {std::int64_t{2}, std::string{"Beta"}, std::monostate{}, std::vector<std::uint8_t>{}}
        };
        Database original;
        assert(original.create_table("Documents", table));
        assert(original.create_index("Documents", "id"));
        assert(original.create_vector_index("Documents", 2));
        assert(original.set_embedding("Documents", 0, {1, 0}));
        assert(original.set_embedding("Documents", 1, {0, 1}));
        original.save(path);

        auto loaded = Database::load(path);
        assert(loaded.table_count() == 1);
        const auto* restored = loaded.find_table("documents");
        assert(restored && restored->rows == table.rows);
        const auto& columns = restored->schema.get_columns();
        assert(columns[0].primary_key && !columns[0].nullable);
        assert(columns[2].declared_type == "REAL");
        assert(columns[2].default_value == Value{1.5});
        assert(loaded.lookup_index("DOCUMENTS", "ID", std::int64_t{2}).value() == std::vector<RowID>{1});
        auto matches = loaded.vector_search("documents", {1, 0}, 2);
        assert(matches.size() == 2 && matches[0].row_id == 0 && matches[1].row_id == 1);
        assert(loaded.insert_row("Documents", {std::int64_t{3}, std::string{"Gamma"}, 3.5,
                                                std::vector<std::uint8_t>{1}}));
        assert(loaded.lookup_index("documents", "id", std::int64_t{3}).value() == std::vector<RowID>{2});
        loaded.save(path);
        assert(Database::load(path).find_table("Documents")->rows.size() == 3);

        std::ofstream corrupt(path, std::ios::binary | std::ios::trunc);
        corrupt << "HDBSNAP1";
        corrupt.close();
        bool rejected = false;
        try { (void)Database::load(path); }
        catch (const std::exception&) { rejected = true; }
        assert(rejected);
        std::filesystem::remove(path);
    } catch (...) {
        std::filesystem::remove(path);
        throw;
    }
    std::cout << "Persistence tests passed.\n";
}
