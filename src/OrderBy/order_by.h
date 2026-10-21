#pragma once

#include "database/TableData/table.h"

#include <string>

bool order_rows(
    Table& table,
    const std::string& column_name,
    bool ascending
);
