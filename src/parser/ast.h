#pragma once

#include "database/TableData/value.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <variant>
#include <utility>

// The parser owns this tree. Column names are resolved against a table by the executor.
struct Expression {
    enum class Kind { Literal, Column, Compare, IsNull, IsNotNull, And, Or, Not } kind;
    Value literal;
    std::string name;
    std::string op;
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
};

struct OrderBy {
    std::string column;
    bool ascending = true;
};

struct SelectItem {
    enum class Kind { Column, Count, Sum, Avg, Min, Max } kind = Kind::Column;
    std::string column; // Empty only for COUNT(*).
};

struct Having {
    SelectItem aggregate;
    std::string op;
    Value literal;
};

struct InnerJoin {
    std::string table;
    std::string left_column;
    std::string right_column;
};

struct SelectStatement {
    bool all_columns = false;
    std::vector<SelectItem> items;
    std::string table;
    std::optional<InnerJoin> join;
    std::unique_ptr<Expression> where;
    std::optional<std::string> group_by;
    std::optional<Having> having;
    std::optional<OrderBy> order_by;
    std::optional<std::size_t> limit;
};

struct ColumnDefinition {
    std::string name;
    std::string type;
    bool nullable = true;
};

struct CreateTableStatement {
    std::string table;
    std::vector<ColumnDefinition> columns;
};

struct InsertStatement {
    std::string table;
    std::vector<std::string> columns; // Empty means schema order.
    std::vector<Value> values;
};

struct UpdateStatement {
    std::string table;
    std::vector<std::pair<std::string, Value>> assignments;
    std::unique_ptr<Expression> where;
};

struct DeleteStatement {
    std::string table;
    std::unique_ptr<Expression> where;
};

using Statement = std::variant<SelectStatement, CreateTableStatement, InsertStatement,
                               UpdateStatement, DeleteStatement>;
