#pragma once

#include "database/TableData/table.h"

#include <string>
#include <vector>

bool order_rows(
    Table& table,
    const std::string& column_name,
    bool ascending
);

bool order_rows(std::vector<const Row*>& rows, const Schema& schema,
                const std::string& column_name, bool ascending);
