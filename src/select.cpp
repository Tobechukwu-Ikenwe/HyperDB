#include "database/TableData/row.h"
#include "database/TableData/schema.h"

#include <string>
#include <vector>

std::vector<Value> select_column(
    const std::vector<Row>& rows,
    const std::string& column_name,
    const Schema& schema
) {
    auto index = schema.get_column_index(column_name);

    if (!index) {
        return {};
    }

    std::vector<Value> selected_values;

    for (const Row& row : rows) {
        if (*index < row.size()) {
            selected_values.push_back(row[*index]);
        }
    }

    return selected_values;
}
