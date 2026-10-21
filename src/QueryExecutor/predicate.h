#pragma once

#include "database/TableData/row.h"
#include "database/TableData/schema.h"
#include "parser/ast.h"

void validate_predicate(const Expression& expression, const Schema& schema);
bool matches_predicate(const Expression& expression, const Row& row, const Schema& schema);
