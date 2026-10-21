#pragma once

#include "value.h"

#include <string>

struct Column {
    std::string name;
    std::string declared_type;
    std::string affinity;

    bool nullable = true;
    bool primary_key = false;
    bool auto_increment = false;

    Value default_value = std::monostate{};
};
