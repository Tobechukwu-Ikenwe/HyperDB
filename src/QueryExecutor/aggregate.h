#pragma once

#include "query_executor.h"
#include "parser/ast.h"

// Receives rows already filtered by WHERE. Validates aggregate semantics and
// produces grouped, ordered, and limited results without changing source rows.
QueryResult aggregate_rows(const SelectStatement& statement,
                           const Schema& schema,
                           const std::vector<const Row*>& filtered);
