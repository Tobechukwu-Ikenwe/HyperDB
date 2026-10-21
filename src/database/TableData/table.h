#pragma once

#include "row.h"
#include "schema.h"

#include <vector>

struct Table {
    Schema schema;
    std::vector<Row> rows;
};
