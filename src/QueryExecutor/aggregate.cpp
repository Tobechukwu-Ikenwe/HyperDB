#include "aggregate.h"

#include "OrderBy/order_by.h"
#include "database/identifier.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace {
bool numeric(const Value& value) {
    return std::holds_alternative<std::int64_t>(value) || std::holds_alternative<double>(value);
}

long double number(const Value& value) {
    return std::holds_alternative<double>(value)
        ? static_cast<long double>(std::get<double>(value))
        : static_cast<long double>(std::get<std::int64_t>(value));
}

bool equal_value(const Value& a, const Value& b) {
    if (numeric(a) && numeric(b)) return number(a) == number(b);
    return a == b;
}

struct ValueHash {
    std::size_t operator()(const Value& value) const {
        if (numeric(value)) return std::hash<long double>{}(number(value)) ^ 0x1234;
        if (std::holds_alternative<std::monostate>(value)) return 0;
        if (std::holds_alternative<std::string>(value))
            return std::hash<std::string>{}(std::get<std::string>(value)) ^ 0x2345;
        std::size_t hash = 0x3456;
        for (auto byte : std::get<std::vector<std::uint8_t>>(value)) hash = hash * 131 + byte;
        return hash;
    }
};

struct ValueEqual {
    bool operator()(const Value& a, const Value& b) const { return equal_value(a, b); }
};

bool less_value(const Value& a, const Value& b) {
    if (numeric(a) && numeric(b)) return number(a) < number(b);
    if (std::holds_alternative<std::string>(a) && std::holds_alternative<std::string>(b))
        return std::get<std::string>(a) < std::get<std::string>(b);
    throw std::runtime_error("Incompatible aggregate comparison types");
}

std::string label(const SelectItem& item) {
    switch (item.kind) {
        case SelectItem::Kind::Column: return item.column;
        case SelectItem::Kind::Count: return "COUNT(" + (item.column.empty() ? "*" : item.column) + ")";
        case SelectItem::Kind::Sum: return "SUM(" + item.column + ")";
        case SelectItem::Kind::Avg: return "AVG(" + item.column + ")";
        case SelectItem::Kind::Min: return "MIN(" + item.column + ")";
        case SelectItem::Kind::Max: return "MAX(" + item.column + ")";
    }
    throw std::runtime_error("Unknown aggregate");
}

Value compute(const SelectItem& item, const Schema& schema,
              const std::vector<const Row*>& rows) {
    const auto index = item.column.empty() ? std::optional<std::size_t>{}
                                           : schema.get_column_index(item.column);
    std::int64_t count = 0;
    long double sum = 0;
    const Value* extreme = nullptr;
    for (const auto* row : rows) {
        const Value* value = index ? &(*row)[*index] : nullptr;
        if (value && std::holds_alternative<std::monostate>(*value)) continue;
        if (value && std::holds_alternative<double>(*value) && !std::isfinite(std::get<double>(*value)))
            throw std::runtime_error("Cannot aggregate non-finite numeric value");
        if (count == std::numeric_limits<std::int64_t>::max())
            throw std::runtime_error("Aggregate row count overflow");
        ++count;
        if (item.kind == SelectItem::Kind::Count) continue;
        if (item.kind == SelectItem::Kind::Sum || item.kind == SelectItem::Kind::Avg) {
            if (!numeric(*value)) throw std::runtime_error("SUM and AVG require numeric values");
            sum += number(*value);
            if (!std::isfinite(sum)) throw std::runtime_error("Aggregate numeric overflow");
        } else if (item.kind == SelectItem::Kind::Min || item.kind == SelectItem::Kind::Max) {
            if (!extreme ||
                (item.kind == SelectItem::Kind::Min && less_value(*value, *extreme)) ||
                (item.kind == SelectItem::Kind::Max && less_value(*extreme, *value))) extreme = value;
        }
    }
    if (item.kind == SelectItem::Kind::Count) return count;
    if (count == 0) return std::monostate{};
    if (item.kind == SelectItem::Kind::Min || item.kind == SelectItem::Kind::Max) return *extreme;
    const auto value = static_cast<double>(item.kind == SelectItem::Kind::Avg ? sum / count : sum);
    if (!std::isfinite(value)) throw std::runtime_error("Aggregate numeric overflow");
    return value;
}

bool compare_having(const Value& a, const std::string& op, const Value& b) {
    if (std::holds_alternative<std::monostate>(a) || std::holds_alternative<std::monostate>(b)) return false;
    const bool equal = equal_value(a, b);
    if (op == "=" || op == "==") return equal;
    if (op == "!=" || op == "<>") return !equal;
    const bool less = less_value(a, b);
    if (op == "<") return less;
    if (op == "<=") return less || equal;
    if (op == ">") return !less && !equal;
    return !less;
}
} // namespace

QueryResult aggregate_rows(const SelectStatement& statement, const Schema& schema,
                           const std::vector<const Row*>& filtered) {
    if (statement.all_columns) throw std::runtime_error("SELECT * cannot be used with aggregates");
    bool has_aggregate = false;
    for (const auto& item : statement.items) {
        if (!item.column.empty() && !schema.get_column_index(item.column))
            throw std::runtime_error("Unknown column: " + item.column);
        if (item.kind == SelectItem::Kind::Column) {
            if (!statement.group_by || normalize_identifier(item.column) !=
                normalize_identifier(*statement.group_by))
                throw std::runtime_error("Selected column must be the GROUP BY column");
        } else has_aggregate = true;
    }
    if (!has_aggregate) throw std::runtime_error("GROUP BY requires an aggregate projection");
    if (statement.having && !statement.having->aggregate.column.empty() &&
        !schema.get_column_index(statement.having->aggregate.column))
        throw std::runtime_error("Unknown HAVING column: " + statement.having->aggregate.column);
    if (statement.order_by && (!statement.group_by ||
        normalize_identifier(statement.order_by->column) !=
        normalize_identifier(*statement.group_by)))
        throw std::runtime_error("Aggregate ORDER BY requires the GROUP BY column");

    struct Group { Value key; std::vector<const Row*> rows; };
    std::vector<Group> groups;
    if (!statement.group_by) groups.push_back({std::monostate{}, {}});
    std::unordered_map<Value, std::size_t, ValueHash, ValueEqual> positions;
    const auto group_index = statement.group_by
        ? schema.get_column_index(*statement.group_by) : std::nullopt;
    for (const auto* row : filtered) {
        std::size_t position = 0;
        if (group_index) {
            const auto& key = (*row)[*group_index];
            if (std::holds_alternative<double>(key) && !std::isfinite(std::get<double>(key)))
                throw std::runtime_error("Cannot group non-finite numeric value");
            auto [it, inserted] = positions.try_emplace(key, groups.size());
            if (inserted) groups.push_back({key, {}});
            position = it->second;
        }
        groups[position].rows.push_back(row);
    }

    QueryResult result;
    for (const auto& item : statement.items) result.columns.push_back(label(item));
    for (const auto& group : groups) {
        if (statement.having && !compare_having(
            compute(statement.having->aggregate, schema, group.rows),
            statement.having->op, statement.having->literal)) continue;
        Row output;
        for (const auto& item : statement.items)
            output.push_back(item.kind == SelectItem::Kind::Column ? group.key
                                                                  : compute(item, schema, group.rows));
        result.rows.push_back(std::move(output));
    }
    if (statement.order_by) {
        Table output;
        for (const auto& name : result.columns) output.schema.add_column({name, "", ""});
        output.rows = std::move(result.rows);
        if (!order_rows(output, statement.order_by->column, statement.order_by->ascending))
            throw std::runtime_error("ORDER BY column must be projected");
        result.rows = std::move(output.rows);
    }
    if (statement.limit && result.rows.size() > *statement.limit) result.rows.resize(*statement.limit);
    return result;
}
