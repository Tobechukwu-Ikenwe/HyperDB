#include "query_executor.h"

#include "aggregate.h"
#include "predicate.h"
#include "database/identifier.h"
#include "OrderBy/order_by.h"
#include "parser/parser.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {
void validate_expression(const Expression& expr, const Schema& schema) {
    if (expr.kind == Expression::Kind::Column && !schema.get_column_index(expr.name))
        throw std::runtime_error("Unknown column: " + expr.name);
    if (expr.left) validate_expression(*expr.left, schema);
    if (expr.right) validate_expression(*expr.right, schema);
}

Value scalar(const Expression& expr, const Row& row, const Schema& schema) {
    if (expr.kind == Expression::Kind::Literal) return expr.literal;
    if (expr.kind == Expression::Kind::Column) return row[*schema.get_column_index(expr.name)];
    throw std::runtime_error("Expected scalar expression");
}

bool numeric(const Value& value) {
    return std::holds_alternative<std::int64_t>(value) || std::holds_alternative<double>(value);
}

long double as_number(const Value& value) {
    return std::holds_alternative<double>(value) ? static_cast<long double>(std::get<double>(value))
                                                  : static_cast<long double>(std::get<std::int64_t>(value));
}

// A missing truth value represents SQL UNKNOWN. WHERE keeps only true rows.
std::optional<bool> evaluate(const Expression& expr, const Row& row, const Schema& schema) {
    using Kind = Expression::Kind;
    if (expr.kind == Kind::Not) {
        auto value = evaluate(*expr.left, row, schema);
        return value ? std::optional<bool>{!*value} : std::nullopt;
    }
    if (expr.kind == Kind::And || expr.kind == Kind::Or) {
        const auto left = evaluate(*expr.left, row, schema);
        const auto right = evaluate(*expr.right, row, schema);
        if (expr.kind == Kind::And) {
            if (left == false || right == false) return false;
            if (!left || !right) return std::nullopt;
            return true;
        }
        if (left == true || right == true) return true;
        if (!left || !right) return std::nullopt;
        return false;
    }
    if (expr.kind == Kind::IsNull || expr.kind == Kind::IsNotNull) {
        const bool is_null = std::holds_alternative<std::monostate>(scalar(*expr.left, row, schema));
        return expr.kind == Kind::IsNull ? is_null : !is_null;
    }
    if (expr.kind != Kind::Compare) throw std::runtime_error("Expected comparison expression");
    const auto left = scalar(*expr.left, row, schema);
    const auto right = scalar(*expr.right, row, schema);
    if (std::holds_alternative<std::monostate>(left) || std::holds_alternative<std::monostate>(right))
        return std::nullopt;
    bool less = false, equal = false, greater = false;
    if (numeric(left) && numeric(right)) {
        const auto a = as_number(left), b = as_number(right);
        if (!std::isfinite(a) || !std::isfinite(b))
            throw std::runtime_error("Cannot compare non-finite numeric value");
        less = a < b; equal = a == b; greater = a > b;
    } else if (std::holds_alternative<std::string>(left) && std::holds_alternative<std::string>(right)) {
        const auto& a = std::get<std::string>(left), &b = std::get<std::string>(right);
        less = a < b; equal = a == b; greater = a > b;
    } else {
        throw std::runtime_error("Incompatible comparison types");
    }
    if (expr.op == "=" || expr.op == "==") return equal;
    if (expr.op == "!=" || expr.op == "<>") return !equal;
    if (expr.op == "<") return less;
    if (expr.op == "<=") return less || equal;
    if (expr.op == ">") return greater;
    return greater || equal;
}

Table inner_join(const Database& database, const std::string& left_name,
                 const Table& left, const InnerJoin& join) {
    const auto* right = database.find_table(join.table);
    if (!right) throw std::runtime_error("Unknown table: " + join.table);
    if (normalize_identifier(left_name) == normalize_identifier(join.table))
        throw std::runtime_error("Self-join requires aliases, which are unsupported");

    Table result;
    std::unordered_map<std::string, std::vector<std::size_t>> bare_names;
    auto append_schema = [&](const std::string& table_name, const Schema& schema) {
        for (const auto& column : schema.get_columns()) {
            Column qualified = column;
            qualified.name = table_name + "." + column.name;
            const auto index = result.schema.size();
            result.schema.add_column(qualified);
            bare_names[normalize_identifier(column.name)].push_back(index);
        }
    };
    append_schema(left_name, left.schema);
    append_schema(join.table, right->schema);
    for (const auto& [name, indices] : bare_names)
        if (indices.size() == 1) result.schema.add_alias(name, indices.front());

    const auto left_key = result.schema.get_column_index(join.left_column);
    const auto right_key = result.schema.get_column_index(join.right_column);
    if (!left_key || !right_key)
        throw std::runtime_error("Unknown or ambiguous JOIN column");
    for (const auto& left_row : left.rows) {
        if (left_row.size() != left.schema.size())
            throw std::runtime_error("Stored row width does not match schema");
        for (const auto& right_row : right->rows) {
            if (right_row.size() != right->schema.size())
                throw std::runtime_error("Stored row width does not match schema");
            Row row = left_row;
            row.insert(row.end(), right_row.begin(), right_row.end());
            const auto& a = row[*left_key];
            const auto& b = row[*right_key];
            if (std::holds_alternative<std::monostate>(a) ||
                std::holds_alternative<std::monostate>(b)) continue;
            if ((std::holds_alternative<double>(a) && !std::isfinite(std::get<double>(a))) ||
                (std::holds_alternative<double>(b) && !std::isfinite(std::get<double>(b))))
                throw std::runtime_error("Cannot join non-finite numeric value");
            bool equal = false;
            if (numeric(a) && numeric(b)) equal = as_number(a) == as_number(b);
            else if (a.index() == b.index()) equal = a == b;
            else throw std::runtime_error("Incompatible JOIN key types");
            if (equal) result.rows.push_back(std::move(row));
        }
    }
    return result;
}

std::optional<std::vector<RowID>> index_candidates(const Database& database,
    const std::string& table, const Expression& expr) {
    if (expr.kind == Expression::Kind::And) {
        auto left = index_candidates(database, table, *expr.left);
        return left ? left : index_candidates(database, table, *expr.right);
    }
    if (expr.kind != Expression::Kind::Compare ||
        (expr.op != "=" && expr.op != "==")) return std::nullopt;
    const Expression* column = nullptr;
    const Expression* literal = nullptr;
    if (expr.left->kind == Expression::Kind::Column &&
        expr.right->kind == Expression::Kind::Literal) {
        column = expr.left.get(); literal = expr.right.get();
    } else if (expr.right->kind == Expression::Kind::Column &&
               expr.left->kind == Expression::Kind::Literal) {
        column = expr.right.get(); literal = expr.left.get();
    }
    return column ? database.lookup_index(table, column->name, literal->literal)
                  : std::nullopt;
}
} // namespace

void validate_predicate(const Expression& expression, const Schema& schema) {
    validate_expression(expression, schema);
}

bool matches_predicate(const Expression& expression, const Row& row, const Schema& schema) {
    return evaluate(expression, row, schema) == true;
}

QueryResult execute_query(const Database& database, const std::string& sql) {
    const auto statement = parse_select(sql);
    const auto* source = database.find_table(statement.table);
    if (!source) throw std::runtime_error("Unknown table: " + statement.table);
    Table joined;
    if (statement.join) {
        joined = inner_join(database, statement.table, *source, *statement.join);
        source = &joined;
    }
    const auto& schema = source->schema;
    if (statement.where) validate_expression(*statement.where, schema);
    if (statement.group_by && !schema.get_column_index(*statement.group_by))
        throw std::runtime_error("Unknown GROUP BY column: " + *statement.group_by);
    if (statement.order_by && !schema.get_column_index(statement.order_by->column))
        throw std::runtime_error("Unknown ORDER BY column: " + statement.order_by->column);

    Table selected;
    selected.schema = schema;
    const auto candidates = !statement.join && statement.where
        ? index_candidates(database, statement.table, *statement.where)
        : std::nullopt;
    auto consider = [&](const Row& row) {
        if (row.size() != schema.size()) throw std::runtime_error("Stored row width does not match schema");
        if (!statement.where || evaluate(*statement.where, row, schema) == true)
            selected.rows.push_back(row);
    };
    if (candidates) {
        for (auto row_id : *candidates) {
            if (row_id >= source->rows.size()) throw std::runtime_error("Index row ID is invalid");
            consider(source->rows[row_id]);
        }
    } else {
        for (const auto& row : source->rows) consider(row);
    }
    bool aggregating = statement.group_by.has_value() || statement.having.has_value();
    for (const auto& item : statement.items)
        aggregating |= item.kind != SelectItem::Kind::Column;
    if (aggregating) return aggregate_rows(statement, schema, selected.rows);

    QueryResult result;
    if (statement.all_columns) {
        for (const auto& column : schema.get_columns()) result.columns.push_back(column.name);
    } else {
        for (const auto& item : statement.items) result.columns.push_back(item.column);
    }
    std::vector<std::size_t> indices;
    for (const auto& name : result.columns) {
        auto index = schema.get_column_index(name);
        if (!index) throw std::runtime_error("Unknown column: " + name);
        indices.push_back(*index);
    }
    if (statement.order_by && !order_rows(selected, statement.order_by->column,
                                           statement.order_by->ascending))
        throw std::runtime_error("Cannot order by column: " + statement.order_by->column);
    const auto count = statement.limit ? std::min(*statement.limit, selected.rows.size()) : selected.rows.size();
    result.rows.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        Row projected;
        projected.reserve(indices.size());
        for (auto index : indices) projected.push_back(selected.rows[i][index]);
        result.rows.push_back(std::move(projected));
    }
    return result;
}
