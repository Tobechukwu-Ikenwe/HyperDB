#pragma once

#include "condition.h"
#include "database/TableData/row.h"
#include "database/TableData/schema.h"

bool matches(
    const Row& row,
    const Condition& condition,
    const Schema& schema
);
