#include "database.h"
#include "filter.h"
#include "identifier.h"

#include <stdexcept>
#include <cmath>
#include <utility>

namespace {
void validate_row(const Row& row, const Schema& schema) {
    if (row.size() != schema.size())
        throw std::invalid_argument("Row width does not match schema");
    const auto& columns = schema.get_columns();
    for (std::size_t i = 0; i < row.size(); ++i) {
        if (!columns[i].nullable && std::holds_alternative<std::monostate>(row[i]))
            throw std::invalid_argument("NULL in non-nullable column: " + columns[i].name);
    }
}

int category(const Value& value) {
    if (std::holds_alternative<std::monostate>(value)) return 0;
    if (std::holds_alternative<std::int64_t>(value)) return 1;
    if (std::holds_alternative<double>(value)) {
        if (!std::isfinite(std::get<double>(value)))
            throw std::invalid_argument("Cannot index non-finite numeric value");
        return 1;
    }
    if (std::holds_alternative<std::string>(value)) return 2;
    return 3;
}
}

bool Database::create_table(
    const std::string& table_name,
    const Table& table
) {
    if (table.schema.size() == 0) throw std::invalid_argument("Table must have columns");
    for (const auto& row : table.rows) validate_row(row, table.schema);
    if (table_name.empty()) throw std::invalid_argument("Table name cannot be empty");
    return tables.emplace(normalize_identifier(table_name), table).second;
}

const Table* Database::find_table(
    const std::string& table_name
) const {
    auto it = tables.find(normalize_identifier(table_name));

    if (it == tables.end()) {
        return nullptr;
    }

    return &it->second;
}

bool Database::insert_row(const std::string& table_name, Row row) {
    const auto key = normalize_identifier(table_name);
    auto it = tables.find(key);
    if (it == tables.end()) return false;
    validate_row(row, it->second.schema);
    auto indexed = indexes.find(key);
    if (indexed != indexes.end()) {
        for (const auto& [column, data] : indexed->second) {
            const auto key_category = category(row[*it->second.schema.get_column_index(column)]);
            if (data.category && key_category && data.category != key_category)
                throw std::invalid_argument("Indexed column has incompatible value type: " + column);
        }
    }
    it->second.rows.push_back(std::move(row));
    if (indexed != indexes.end()) {
        try {
            for (auto& [column, data] : indexed->second) {
                const auto& key = it->second.rows.back()[*it->second.schema.get_column_index(column)];
                data.tree.insert(key, it->second.rows.size() - 1);
                if (!data.category) data.category = category(key);
            }
        } catch (...) {
            indexes.erase(key);
            it->second.rows.pop_back();
            throw;
        }
    }
    return true;
}

bool Database::replace_rows(const std::string& table_name, std::vector<Row> rows,
                            const std::vector<RowID>& old_row_ids) {
    const auto key = normalize_identifier(table_name);
    auto existing = tables.find(key);
    if (existing == tables.end()) return false;
    if (rows.size() != old_row_ids.size())
        throw std::invalid_argument("Row ID mapping size does not match replacement rows");
    for (std::size_t i = 0; i < old_row_ids.size(); ++i) {
        if (old_row_ids[i] >= existing->second.rows.size() ||
            (i && old_row_ids[i] <= old_row_ids[i - 1]))
            throw std::invalid_argument("Replacement row IDs must be valid and increasing");
    }
    Table replacement{existing->second.schema, std::move(rows)};
    Database staged;
    staged.create_table(key, replacement);
    if (auto found = indexes.find(key); found != indexes.end())
        for (const auto& [column, index] : found->second)
            staged.create_index(key, column);
    if (auto found = vector_indexes.find(key); found != vector_indexes.end()) {
        staged.create_vector_index(key, found->second.dimensions());
        for (std::size_t i = 0; i < old_row_ids.size(); ++i) {
            if (auto embedding = found->second.entries().find(old_row_ids[i]);
                embedding != found->second.entries().end())
                staged.set_embedding(key, i, embedding->second);
        }
    }
    using std::swap;
    swap(existing->second, staged.tables.at(key));
    if (auto found = indexes.find(key); found != indexes.end() && !found->second.empty())
        found->second.swap(staged.indexes.at(key));
    if (auto found = vector_indexes.find(key); found != vector_indexes.end())
        swap(found->second, staged.vector_indexes.at(key));
    return true;
}

bool Database::create_index(const std::string& table_name, const std::string& column_name) {
    const auto key = normalize_identifier(table_name);
    auto table = tables.find(key);
    if (table == tables.end()) return false;
    auto column = table->second.schema.get_column_index(column_name);
    const auto column_key = normalize_identifier(column_name);
    if (!column) return false;
    if (auto found = indexes.find(key); found != indexes.end() &&
        found->second.contains(column_key)) return false;
    IndexData data;
    for (std::size_t i = 0; i < table->second.rows.size(); ++i) {
        const auto& row = table->second.rows[i];
        validate_row(row, table->second.schema);
        const auto key_category = category(row[*column]);
        if (data.category && key_category && data.category != key_category)
            throw std::invalid_argument("Indexed column has incompatible value types");
        if (key_category) data.category = key_category;
        data.tree.insert(row[*column], i);
    }
    indexes[key].emplace(column_key, std::move(data));
    return true;
}

std::optional<std::vector<RowID>> Database::lookup_index(const std::string& table_name,
    const std::string& column_name, const Value& key) const {
    auto table = indexes.find(normalize_identifier(table_name));
    if (table == indexes.end()) return std::nullopt;
    auto index = table->second.find(normalize_identifier(column_name));
    if (index == table->second.end()) return std::nullopt;
    const auto key_category = category(key);
    if (index->second.category && key_category && index->second.category != key_category)
        return std::nullopt;
    auto found = index->second.tree.search(key);
    return found.value_or(std::vector<RowID>{});
}

bool Database::create_vector_index(const std::string& table_name, std::size_t dimensions) {
    const auto key = normalize_identifier(table_name);
    if (!tables.contains(key) || vector_indexes.contains(key)) return false;
    vector_indexes.emplace(key, VectorIndex(dimensions));
    return true;
}

bool Database::set_embedding(const std::string& table_name, RowID row_id,
                             std::vector<double> embedding) {
    const auto key = normalize_identifier(table_name);
    auto table = tables.find(key);
    auto index = vector_indexes.find(key);
    if (table == tables.end() || index == vector_indexes.end() ||
        row_id >= table->second.rows.size()) return false;
    index->second.upsert(row_id, std::move(embedding));
    return true;
}

std::vector<VectorMatch> Database::vector_search(const std::string& table_name,
    const std::vector<double>& query, std::size_t limit,
    const std::optional<Condition>& metadata_filter) const {
    const auto key = normalize_identifier(table_name);
    auto table = tables.find(key);
    auto index = vector_indexes.find(key);
    if (table == tables.end() || index == vector_indexes.end())
        throw std::runtime_error("Vector index does not exist for table: " + table_name);
    if (metadata_filter && !table->second.schema.get_column_index(metadata_filter->column_name))
        throw std::runtime_error("Unknown metadata column: " + metadata_filter->column_name);
    if (metadata_filter && metadata_filter->op != '=')
        throw std::runtime_error("Vector metadata filter supports equality only");
    return index->second.search(query, limit, [&](RowID row_id) {
        return !metadata_filter || matches(table->second.rows[row_id],
                                            *metadata_filter, table->second.schema);
    });
}

bool Database::drop_table(
    const std::string& table_name
) {
    const auto key = normalize_identifier(table_name);
    indexes.erase(key);
    vector_indexes.erase(key);
    return tables.erase(key) > 0;
}

std::size_t Database::table_count() const {
    return tables.size();
}
