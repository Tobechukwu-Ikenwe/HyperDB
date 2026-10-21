#pragma once

#include "database/TableData/value.h"

#include <string>

struct Condition {
    std::string column_name;
    char op;
    Value value;
};
