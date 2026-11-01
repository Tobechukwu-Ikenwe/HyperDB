#include "QueryExecutor/query_executor.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <variant>

int main() {
    try {
        Database database;
        (void)execute_statement(database, "CREATE TABLE greeting (message TEXT)");
        (void)execute_statement(database,
                                "INSERT INTO greeting VALUES ('Hello from C++')");
        const auto result = execute_statement(database,
                                              "SELECT message FROM greeting");
        if (!result.query.rows.empty() &&
            std::holds_alternative<std::string>(result.query.rows[0][0]))
            std::cout << std::get<std::string>(result.query.rows[0][0]) << '\n';
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
