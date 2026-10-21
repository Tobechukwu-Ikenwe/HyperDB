#include "limit.h"

std::vector<Row> limit_rows(
    const std::vector<Row>& rows,
    std::size_t limit_count
) {
    if (limit_count >= rows.size()) {
        return rows;
    }

    return std::vector<Row>(
        rows.begin(),
        rows.begin() + limit_count
    );
}
