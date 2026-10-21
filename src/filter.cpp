#include "filter.h"

#include <cstdint>
#include <string>

bool matches(
    const Row& row,
    const Condition& condition,
    const Schema& schema
) {
    auto index = schema.get_column_index(condition.column_name);

    if (!index || *index >= row.size()) {
        return false;
    }

    const Value& actual = row[*index];
    const Value& expected = condition.value;

    if (std::holds_alternative<std::int64_t>(actual) &&
        std::holds_alternative<std::int64_t>(expected)) {

        auto a = std::get<std::int64_t>(actual);
        auto b = std::get<std::int64_t>(expected);

        switch (condition.op) {
            case '=': return a == b;
            case '<': return a < b;
            case '>': return a > b;
            default: return false;
        }
    }

    if (std::holds_alternative<double>(actual) &&
        std::holds_alternative<double>(expected)) {

        auto a = std::get<double>(actual);
        auto b = std::get<double>(expected);

        switch (condition.op) {
            case '=': return a == b;
            case '<': return a < b;
            case '>': return a > b;
            default: return false;
        }
    }

    if (std::holds_alternative<std::string>(actual) &&
        std::holds_alternative<std::string>(expected)) {

        const auto& a = std::get<std::string>(actual);
        const auto& b = std::get<std::string>(expected);

        if (condition.op == '=') {
            return a == b;
        }
    }

    return false;
}
