#pragma once

#include "database/TableData/row.h"

#include <cstddef>
#include <vector>

std::vector<Row> limit_rows(
    const std::vector<Row>& rows,
    std::size_t limit_count
);
