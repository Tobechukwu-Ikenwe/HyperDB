#pragma once

#include "condition.h"
#include "database/TableData/row.h"
#include "database/TableData/schema.h"

#include <vector>

std::vector<Row> filter_table(
    const std::vector<Row>& rows,
    const Condition& condition,
    const Schema& schema
);
