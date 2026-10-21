#include "filter_table.h"

#include "filter.h"

std::vector<Row> filter_table(
    const std::vector<Row>& rows,
    const Condition& condition,
    const Schema& schema
) {
    std::vector<Row> result;

    for (const Row& row : rows) {
        if (matches(row, condition, schema)) {
            result.push_back(row);
        }
    }

    return result;
}
