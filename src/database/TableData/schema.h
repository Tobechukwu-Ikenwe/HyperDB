#pragma once

#include "column.h"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class Schema {
private:
    std::vector<Column> columns;
    std::unordered_map<std::string, std::size_t> column_indices;

public:
    void add_column(const Column& column);

    // Adds a lookup name for an existing column without changing row width.
    void add_alias(const std::string& alias, std::size_t index);

    std::optional<std::size_t> get_column_index(
        const std::string& column_name
    ) const;

    const Column* get_column(
        const std::string& column_name
    ) const;

    const std::vector<Column>& get_columns() const;

    std::size_t size() const;
};
