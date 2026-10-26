#include "query_executor.h"

#include "predicate.h"
#include "parser/parser.h"

#include <stdexcept>
#include <unordered_set>

StatementResult execute_statement(Database& database, const std::string& sql) {
    auto statement = parse_statement(sql);
    if (std::holds_alternative<SelectStatement>(statement))
        return {execute_query(database, std::get<SelectStatement>(statement)), 0, false, false};

    if (const auto* create = std::get_if<CreateTableStatement>(&statement)) {
        Table table;
        for (const auto& definition : create->columns) {
            Column column;
            column.name = definition.name;
            column.declared_type = definition.type;
            column.affinity = definition.type; // Metadata only; no implicit conversion.
            column.nullable = definition.nullable;
            table.schema.add_column(column);
        }
        if (!database.create_table(create->table, table))
            throw std::runtime_error("Table already exists: " + create->table);
        return {{}, 0, true, true};
    }

    if (const auto* update = std::get_if<UpdateStatement>(&statement)) {
        const auto* table = database.find_table(update->table);
        if (!table) throw std::runtime_error("Unknown table: " + update->table);
        if (update->where) validate_predicate(*update->where, table->schema);
        std::unordered_set<std::size_t> assigned;
        std::vector<std::pair<std::size_t, Value>> assignments;
        for (const auto& [name, value] : update->assignments) {
            const auto index = table->schema.get_column_index(name);
            if (!index) throw std::runtime_error("Unknown UPDATE column: " + name);
            if (!assigned.insert(*index).second)
                throw std::runtime_error("Duplicate UPDATE column: " + name);
            assignments.emplace_back(*index, value);
        }
        auto rows = table->rows;
        std::vector<RowID> old_ids;
        old_ids.reserve(rows.size());
        std::size_t affected = 0;
        for (std::size_t i = 0; i < rows.size(); ++i) {
            old_ids.push_back(i);
            if (!update->where || matches_predicate(*update->where, rows[i], table->schema)) {
                for (const auto& [index, value] : assignments) rows[i][index] = value;
                ++affected;
            }
        }
        if (affected) database.replace_rows(update->table, std::move(rows), old_ids);
        return {{}, affected, true, affected != 0};
    }

    if (const auto* removal = std::get_if<DeleteStatement>(&statement)) {
        const auto* table = database.find_table(removal->table);
        if (!table) throw std::runtime_error("Unknown table: " + removal->table);
        if (removal->where) validate_predicate(*removal->where, table->schema);
        std::size_t first_removed = 0;
        while (first_removed < table->rows.size() && removal->where &&
               !matches_predicate(*removal->where, table->rows[first_removed], table->schema))
            ++first_removed;
        if (first_removed == table->rows.size()) return {{}, 0, true, false};
        std::vector<Row> survivors;
        std::vector<RowID> old_ids;
        survivors.reserve(table->rows.size() - 1);
        old_ids.reserve(table->rows.size() - 1);
        for (std::size_t i = 0; i < first_removed; ++i) {
            survivors.push_back(table->rows[i]);
            old_ids.push_back(i);
        }
        for (std::size_t i = first_removed + 1; i < table->rows.size(); ++i) {
            if (!removal->where || matches_predicate(*removal->where, table->rows[i], table->schema))
                continue;
            survivors.push_back(table->rows[i]);
            old_ids.push_back(i);
        }
        const auto affected = table->rows.size() - survivors.size();
        if (affected) database.replace_rows(removal->table, std::move(survivors), old_ids);
        return {{}, affected, true, affected != 0};
    }

    const auto& insert = std::get<InsertStatement>(statement);
    const auto* table = database.find_table(insert.table);
    if (!table) throw std::runtime_error("Unknown table: " + insert.table);
    Row row;
    if (insert.columns.empty()) {
        row = insert.values;
    } else {
        if (insert.columns.size() != insert.values.size())
            throw std::runtime_error("INSERT column/value count mismatch");
        for (const auto& column : table->schema.get_columns())
            row.push_back(column.default_value);
        std::unordered_set<std::size_t> supplied;
        for (std::size_t i = 0; i < insert.columns.size(); ++i) {
            auto index = table->schema.get_column_index(insert.columns[i]);
            if (!index) throw std::runtime_error("Unknown INSERT column: " + insert.columns[i]);
            if (!supplied.insert(*index).second)
                throw std::runtime_error("Duplicate INSERT column: " + insert.columns[i]);
            row[*index] = insert.values[i];
        }
    }
    if (!database.insert_row(insert.table, std::move(row)))
        throw std::runtime_error("Unknown table: " + insert.table);
    return {{}, 1, true, true};
}
