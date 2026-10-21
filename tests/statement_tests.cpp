#include "QueryExecutor/query_executor.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

void fails(Database& database, const std::string& sql) {
    bool failed = false;
    try { (void)execute_statement(database, sql); }
    catch (const std::exception&) { failed = true; }
    assert(failed);
}

int main() {
    Database database;
    auto created = execute_statement(database, "CREATE TABLE people (id INTEGER NOT NULL, name TEXT, score REAL)");
    assert(created.modified && created.rows_affected == 0);
    fails(database, "CREATE TABLE people (id INTEGER)");
    fails(database, "CREATE TABLE other (id INTEGER, ID TEXT)");
    fails(database, "INSERT INTO missing VALUES (1)");
    fails(database, "INSERT INTO people (name) VALUES ('Missing ID')");
    fails(database, "INSERT INTO people VALUES (1)");
    fails(database, "INSERT INTO people (id, id) VALUES (1, 2)");
    fails(database, "INSERT INTO people (missing) VALUES (1)");
    assert(database.find_table("people")->rows.empty());

    auto inserted = execute_statement(database, "INSERT INTO people (name, id) VALUES ('Ada', 7)");
    assert(inserted.modified && inserted.rows_affected == 1);
    inserted = execute_statement(database, "INSERT INTO people VALUES (8, 'Grace', 9.5)");
    assert(inserted.rows_affected == 1);
    auto selected = execute_statement(database, "SELECT id, name FROM people WHERE score IS NULL ORDER BY id");
    assert(!selected.modified && selected.query.rows.size() == 1);
    assert(std::get<std::string>(selected.query.rows[0][1]) == "Ada");
    assert(database.create_index("people", "id"));
    assert(database.create_vector_index("people", 2));
    assert(database.set_embedding("people", 0, {1, 0}));
    assert(database.set_embedding("people", 1, {0, 1}));
    fails(database, "UPDATE people SET id = NULL WHERE id = 7");
    assert(database.lookup_index("people", "id", std::int64_t{7}).value() == std::vector<RowID>{0});
    fails(database, "UPDATE people SET id = 9, ID = 10 WHERE id = 7");
    fails(database, "UPDATE people SET missing = 1");
    auto changed = execute_statement(database, "UPDATE people SET id = 9, name = 'Ada Lovelace' WHERE id = 7");
    assert(changed.modified && changed.rows_affected == 1);
    assert(database.lookup_index("people", "id", std::int64_t{7}).value().empty());
    assert(database.lookup_index("people", "id", std::int64_t{9}).value() == std::vector<RowID>{0});
    assert(database.vector_search("people", {1, 0}, 1)[0].row_id == 0);
    changed = execute_statement(database, "DELETE FROM people WHERE id = 9");
    assert(changed.modified && changed.rows_affected == 1);
    assert(database.lookup_index("people", "id", std::int64_t{8}).value() == std::vector<RowID>{0});
    assert(database.vector_search("people", {0, 1}, 1)[0].row_id == 0);
    auto no_match = execute_statement(database, "DELETE FROM people WHERE id = 999");
    assert(no_match.is_write && !no_match.modified && no_match.rows_affected == 0);

    const auto path = std::filesystem::temp_directory_path() /
        ("hyperdb-statement-test-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        database.save(path);
        auto restored = Database::load(path);
        assert(restored.find_table("people")->rows.size() == 1);
        assert(restored.lookup_index("people", "id", std::int64_t{8}).value() == std::vector<RowID>{0});
        assert(restored.vector_search("people", {0, 1}, 1)[0].row_id == 0);
        assert(execute_statement(restored, "SELECT name FROM people WHERE id = 8").query.rows.size() == 1);
        std::filesystem::remove(path);
    } catch (...) {
        std::filesystem::remove(path);
        throw;
    }
    std::cout << "Statement tests passed.\n";
}
