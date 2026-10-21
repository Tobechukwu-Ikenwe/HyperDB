#pragma once

#include "database/database.h"

#include <string>

const Table* find_table(
    const Database& database,
    const std::string& table_name
);
