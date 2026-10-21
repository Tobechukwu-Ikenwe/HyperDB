#pragma once

#include "database/database.h"
#include "database/TableData/row.h"

#include <string>
#include <vector>

struct QueryResult {
    std::vector<std::string> columns;
    std::vector<Row> rows;
};

QueryResult execute_query(
    const Database& database,
    const std::string& sql
);

struct StatementResult {
    QueryResult query;
    std::size_t rows_affected = 0;
    bool is_write = false;
    bool modified = false;
};

// Executes one supported statement. SELECT returns query rows; writes set modified.
StatementResult execute_statement(Database& database, const std::string& sql);
