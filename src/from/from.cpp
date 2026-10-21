#include "from.h"

const Table* find_table(
    const Database& database,
    const std::string& table_name
) {
    return database.find_table(table_name);
}
