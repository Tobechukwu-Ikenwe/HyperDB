#include "schema.h"
#include "../identifier.h"

#include <stdexcept>

void Schema::add_column(const Column& column) {
    const auto key = normalize_identifier(column.name);
    if (column_indices.contains(key)) {
        throw std::invalid_argument("Duplicate column: " + column.name);
    }
    if (column.name.empty()) throw std::invalid_argument("Column name cannot be empty");

    column_indices.emplace(key, columns.size());
    columns.push_back(column);
}

void Schema::add_alias(const std::string& alias, std::size_t index) {
    const auto key = normalize_identifier(alias);
    if (index >= columns.size() || alias.empty() || column_indices.contains(key))
        throw std::invalid_argument("Invalid or duplicate column alias: " + alias);
    column_indices.emplace(key, index);
}

std::optional<std::size_t> Schema::get_column_index(
    const std::string& column_name
) const {
    auto it = column_indices.find(normalize_identifier(column_name));

    if (it == column_indices.end()) {
        return std::nullopt;
    }

    return it->second;
}

const Column* Schema::get_column(
    const std::string& column_name
) const {
    auto index = get_column_index(column_name);

    if (!index) {
        return nullptr;
    }

    return &columns[*index];
}

const std::vector<Column>& Schema::get_columns() const {
    return columns;
}

std::size_t Schema::size() const {
    return columns.size();
}
