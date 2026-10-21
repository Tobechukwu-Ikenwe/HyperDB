#pragma once

#include "row_id.h"
#include "../TableData/value.h"

#include <vector>

struct IndexEntry {
    Value key;
    std::vector<RowID> row_ids;
};
