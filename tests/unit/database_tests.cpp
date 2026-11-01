#include "database/database.h"
#include "database/TableData/column.h"
#include "database/TableData/table.h"
#include "filter.h"
#include "filter_table.h"
#include "Limit/limit.h"
#include "OrderBy/order_by.h"
#include "QueryExecutor/query_executor.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <stdexcept>

namespace {
void expect_failure(Database& database, const std::string& sql, const std::string& message) {
    try { (void)execute_query(database, sql); }
    catch (const std::runtime_error& error) {
        assert(std::string(error.what()).find(message) != std::string::npos);
        return;
    }
    assert(false && "Expected query failure");
}
}

int main() {
    Table table;

    table.schema.add_column({
        "id",
        "INTEGER",
        "INTEGER",
        false,
        true,
        false,
        std::monostate{}
    });

    table.schema.add_column({
        "name",
        "TEXT",
        "TEXT",
        false,
        false,
        false,
        std::monostate{}
    });

    table.schema.add_column({
        "budget",
        "REAL",
        "REAL",
        false,
        false,
        false,
        std::monostate{}
    });

    table.rows = {
        {std::int64_t{1}, std::string{"Finance"}, 5.2},
        {std::int64_t{2}, std::string{"Biology"}, 3.7},
        {std::int64_t{3}, std::string{"Physics"}, 6.1}
    };

    assert(table.schema.size() == 3);

    auto budget_index =
        table.schema.get_column_index("budget");

    assert(budget_index);
    assert(*budget_index == 2);

    auto missing =
        table.schema.get_column_index("missing");

    assert(!missing);

    Condition condition{
        "budget",
        '>',
        5.0
    };

    auto filtered =
        filter_table(
            table.rows,
            condition,
            table.schema
        );

    assert(filtered.size() == 2);

    auto limited =
        limit_rows(table.rows, 2);

    assert(limited.size() == 2);

    assert(order_rows(table, "budget", true));

    assert(
        std::get<double>(table.rows.front()[2])
        == 3.7
    );

    Database database;

    assert(
        database.create_table("Department", table)
    );

    assert(database.table_count() == 1);

    Table bad = table;
    bad.rows.push_back({std::int64_t{4}});
    try { (void)database.create_table("Bad", bad); assert(false); }
    catch (const std::invalid_argument&) {}
    bad = table;
    bad.rows[0][0] = std::monostate{};
    try { (void)database.create_table("Bad", bad); assert(false); }
    catch (const std::invalid_argument&) {}
    try { bad.schema.add_column(bad.schema.get_columns()[0]); assert(false); }
    catch (const std::invalid_argument&) {}

    assert(database.find_table("Department") != nullptr);

    const auto rows_before = database.find_table("Department")->rows;
    auto result = execute_query(database,
        "SELECT name, budget FROM Department WHERE budget > 4 AND NOT name = 'Biology' ORDER BY budget DESC LIMIT 1;");
    assert((result.columns == std::vector<std::string>{"name", "budget"}));
    assert(result.rows.size() == 1);
    assert(std::get<std::string>(result.rows[0][0]) == "Physics");
    assert(execute_query(database, "select NAME from department limit 1").rows.size() == 1);
    assert(database.find_table("Department")->rows == rows_before);

    result = execute_query(database, "SELECT * FROM Department WHERE budget = NULL");
    assert(result.columns.size() == 3 && result.rows.empty());
    result = execute_query(database, "SELECT name FROM Department LIMIT 0");
    assert(result.rows.empty());
    expect_failure(database, "SELECT missing FROM Department LIMIT 0", "Unknown column");
    result = execute_query(database, "SELECT name FROM Department WHERE budget >= 5 OR name = 'Biology' ORDER BY budget ASC");
    assert(result.rows.size() == 3);
    assert(std::get<std::string>(result.rows.front()[0]) == "Biology");
    Table mixed;
    mixed.schema.add_column({"amount", "REAL", "REAL"});
    mixed.rows = {{3.5}, {std::int64_t{2}}, {std::monostate{}}, {3.0}};
    assert(database.create_table("Mixed", mixed));
    auto numeric_result = execute_query(database, "SELECT amount FROM Mixed ORDER BY amount ASC");
    assert(std::holds_alternative<std::monostate>(numeric_result.rows[0][0]));
    assert(std::get<std::int64_t>(numeric_result.rows[1][0]) == 2);
    assert(std::get<double>(numeric_result.rows[2][0]) == 3.0);
    assert(std::get<double>(numeric_result.rows[3][0]) == 3.5);
    assert(execute_query(database, "SELECT amount FROM Mixed WHERE amount IS NULL").rows.size() == 1);
    assert(execute_query(database, "SELECT amount FROM Mixed WHERE amount IS NOT NULL").rows.size() == 3);
    assert(execute_query(database, "SELECT amount FROM Mixed WHERE NOT amount IS NULL").rows.size() == 3);
    assert(execute_query(database, "SELECT amount FROM Mixed WHERE amount = NULL OR amount IS NULL").rows.size() == 1);
    assert(execute_query(database, "SELECT amount FROM Mixed WHERE NULL IS NULL").rows.size() == 4);
    assert(database.insert_row("Mixed", {4.0}));
    try { (void)database.insert_row("Mixed", {}); assert(false); }
    catch (const std::invalid_argument&) {}
    assert(database.find_table("Mixed")->rows.size() == 5);
    assert(!database.insert_row("Missing", {1.0}));
    assert(database.drop_table("Mixed"));
    expect_failure(database, "SELECT missing FROM Department", "Unknown column");
    expect_failure(database, "SELECT name FROM missing", "Unknown table");
    expect_failure(database, "SELECT name FROM Department WHERE missing = 1", "Unknown column");
    expect_failure(database, "SELECT name FROM Department ORDER BY missing", "Unknown ORDER BY column");
    expect_failure(database, "SELECT name FROM Department WHERE name = 1", "Incompatible comparison types");

    Table sales;
    sales.schema.add_column({"category", "TEXT", "TEXT"});
    sales.schema.add_column({"amount", "REAL", "REAL"});
    sales.rows = {
        {std::string{"A"}, 10.0}, {std::string{"A"}, 20.0},
        {std::string{"A"}, std::monostate{}}, {std::string{"B"}, 5.0},
        {std::string{"B"}, 5.0}, {std::string{"C"}, std::monostate{}},
        {std::monostate{}, 3.0}
    };
    assert(database.create_table("Sales", sales));
    auto aggregate = execute_query(database,
        "SELECT category, COUNT(*), COUNT(amount), SUM(amount), AVG(amount), MIN(amount), MAX(amount) "
        "FROM Sales GROUP BY category HAVING COUNT(*) > 2 ORDER BY category ASC");
    assert(aggregate.rows.size() == 1 && aggregate.columns.size() == 7);
    assert(std::get<std::string>(aggregate.rows[0][0]) == "A");
    assert(std::get<std::int64_t>(aggregate.rows[0][1]) == 3);
    assert(std::get<std::int64_t>(aggregate.rows[0][2]) == 2);
    assert(std::get<double>(aggregate.rows[0][3]) == 30.0);
    assert(std::get<double>(aggregate.rows[0][4]) == 15.0);
    assert(std::get<double>(aggregate.rows[0][5]) == 10.0);
    assert(std::get<double>(aggregate.rows[0][6]) == 20.0);
    aggregate = execute_query(database, "SELECT COUNT(*), SUM(amount), AVG(amount) FROM Sales");
    assert(aggregate.rows.size() == 1);
    assert(std::get<std::int64_t>(aggregate.rows[0][0]) == 7);
    assert(std::get<double>(aggregate.rows[0][1]) == 43.0);
    assert(std::get<double>(aggregate.rows[0][2]) == 8.6);
    aggregate = execute_query(database, "SELECT COUNT(*), SUM(amount) FROM Sales WHERE amount > 100");
    assert(aggregate.rows.size() == 1);
    assert(std::get<std::int64_t>(aggregate.rows[0][0]) == 0);
    assert(std::holds_alternative<std::monostate>(aggregate.rows[0][1]));
    aggregate = execute_query(database, "SELECT category, COUNT(*) FROM Sales GROUP BY category ORDER BY category ASC LIMIT 1");
    assert(aggregate.rows.size() == 1 && std::holds_alternative<std::monostate>(aggregate.rows[0][0]));
    expect_failure(database, "SELECT category, COUNT(*) FROM Sales", "GROUP BY");
    expect_failure(database, "SELECT category FROM Sales GROUP BY category", "requires an aggregate");
    expect_failure(database, "SELECT SUM(category) FROM Sales", "require numeric");
    assert(database.drop_table("Sales"));

    Table employee;
    employee.schema.add_column({"dept_id", "INTEGER", "INTEGER"});
    employee.schema.add_column({"name", "TEXT", "TEXT"});
    employee.rows = {
        {std::int64_t{1}, std::string{"Ada"}},
        {std::int64_t{1}, std::string{"Lin"}},
        {std::int64_t{2}, std::string{"Sam"}},
        {std::monostate{}, std::string{"No department"}}
    };
    assert(database.create_table("Employee", employee));
    auto joined = execute_query(database,
        "SELECT Department.name, Employee.name FROM Department INNER JOIN Employee "
        "ON Department.id = Employee.dept_id WHERE Employee.name = 'Ada' ORDER BY Department.name");
    assert(joined.rows.size() == 1 && joined.columns.size() == 2);
    assert(std::get<std::string>(joined.rows[0][0]) == "Finance");
    assert(std::get<std::string>(joined.rows[0][1]) == "Ada");
    joined = execute_query(database,
        "SELECT Department.name, COUNT(*) FROM Department JOIN Employee "
        "ON Department.id = Employee.dept_id GROUP BY Department.name "
        "HAVING COUNT(*) > 1 ORDER BY Department.name");
    assert(joined.rows.size() == 1);
    assert(std::get<std::int64_t>(joined.rows[0][1]) == 2);
    joined = execute_query(database,
        "SELECT * FROM Department JOIN Employee ON Department.id = Employee.dept_id LIMIT 1");
    assert(joined.columns.size() == 5 && joined.rows.size() == 1);
    expect_failure(database,
        "SELECT name FROM Department JOIN Employee ON Department.id = Employee.dept_id",
        "Unknown column");
    assert(database.drop_table("Employee"));

    assert(
        database.find_table("Missing") == nullptr
    );

    assert(database.drop_table("Department"));

    assert(database.table_count() == 0);

    std::cout << "All HyperDB tests passed.\n";

    return 0;
}
