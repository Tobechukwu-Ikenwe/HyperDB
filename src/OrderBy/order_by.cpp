#include "order_by.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <stdexcept>
#include <string>

namespace {

bool less_value(
    const Value& a,
    const Value& b
) {
    const bool a_numeric = std::holds_alternative<std::int64_t>(a) || std::holds_alternative<double>(a);
    const bool b_numeric = std::holds_alternative<std::int64_t>(b) || std::holds_alternative<double>(b);
    if (a_numeric && b_numeric) {
        const long double left = std::holds_alternative<std::int64_t>(a)
            ? static_cast<long double>(std::get<std::int64_t>(a))
            : static_cast<long double>(std::get<double>(a));
        const long double right = std::holds_alternative<std::int64_t>(b)
            ? static_cast<long double>(std::get<std::int64_t>(b))
            : static_cast<long double>(std::get<double>(b));
        return left < right;
    }
    if (a.index() != b.index()) {
        return a.index() < b.index();
    }

    if (std::holds_alternative<std::int64_t>(a)) {
        return std::get<std::int64_t>(a) <
               std::get<std::int64_t>(b);
    }

    if (std::holds_alternative<double>(a)) {
        return std::get<double>(a) <
               std::get<double>(b);
    }

    if (std::holds_alternative<std::string>(a)) {
        return std::get<std::string>(a) <
               std::get<std::string>(b);
    }

    return false;
}

}

bool order_rows(
    Table& table,
    const std::string& column_name,
    bool ascending
) {
    auto index = table.schema.get_column_index(column_name);

    if (!index) {
        return false;
    }

    for (const auto& row : table.rows) {
        if (*index >= row.size()) throw std::runtime_error("Stored row width does not match schema");
        if (std::holds_alternative<double>(row[*index]) && !std::isfinite(std::get<double>(row[*index])))
            throw std::runtime_error("Cannot order non-finite numeric value");
    }

    std::stable_sort(
        table.rows.begin(),
        table.rows.end(),
        [index, ascending](const Row& a, const Row& b) {
            if (ascending) {
                return less_value(a[*index], b[*index]);
            }

            return less_value(b[*index], a[*index]);
        }
    );

    return true;
}

bool order_rows(std::vector<const Row*>& rows, const Schema& schema,
                const std::string& column_name, bool ascending) {
    const auto index = schema.get_column_index(column_name);
    if (!index) return false;
    for (const auto* row : rows) {
        if (*index >= row->size()) throw std::runtime_error("Stored row width does not match schema");
        if (std::holds_alternative<double>((*row)[*index]) &&
            !std::isfinite(std::get<double>((*row)[*index])))
            throw std::runtime_error("Cannot order non-finite numeric value");
    }
    std::stable_sort(rows.begin(), rows.end(), [index, ascending](const Row* a, const Row* b) {
        return ascending ? less_value((*a)[*index], (*b)[*index])
                         : less_value((*b)[*index], (*a)[*index]);
    });
    return true;
}
