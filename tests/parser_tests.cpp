#include "parser/parser.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}

void fails(const std::string& sql, const std::string& expected) {
    try { (void)parse_select(sql); }
    catch (const std::runtime_error& error) {
        check(std::string(error.what()).find(expected) != std::string::npos,
              "Wrong parse error for: " + sql + ": " + error.what());
        return;
    }
    check(false, "Expected parse error for: " + sql);
}

int main() {
    auto query = parse_select("SELECT name, budget FROM Department WHERE id = 1 OR id = 2 AND NOT budget < 5 ORDER BY budget DESC LIMIT 2;");
    check(query.table == "Department" && query.items.size() == 2, "Projection/table");
    check(query.where && query.where->kind == Expression::Kind::Or, "OR precedence");
    check(query.where->right->kind == Expression::Kind::And, "AND precedence");
    check(query.where->right->right->kind == Expression::Kind::Not, "NOT precedence");
    check(query.order_by && !query.order_by->ascending && query.limit == 2, "Order/limit");
    auto quoted = parse_select("SELECT \"select\" FROM [my table] WHERE \"select\" = 'it''s';");
    check(quoted.items[0].column == "select" && quoted.table == "my table", "Quoted names");
    check(std::get<std::string>(quoted.where->right->literal) == "it's", "Escaped string");
    check(parse_select("select * from t limit 0").limit == 0, "Zero limit");
    auto null_check = parse_select("SELECT x FROM t WHERE x IS NULL OR NOT y IS NOT NULL");
    check(null_check.where->kind == Expression::Kind::Or &&
          null_check.where->left->kind == Expression::Kind::IsNull &&
          null_check.where->right->kind == Expression::Kind::Not &&
          null_check.where->right->left->kind == Expression::Kind::IsNotNull,
          "IS NULL precedence");
    auto grouped = parse_select("SELECT category, COUNT(*), SUM(amount) FROM sales WHERE amount > 0 GROUP BY category HAVING COUNT(*) > 1 ORDER BY category ASC LIMIT 3");
    check(grouped.items.size() == 3 && grouped.items[1].kind == SelectItem::Kind::Count, "Aggregate projection");
    check(grouped.group_by == "category" && grouped.having && grouped.having->op == ">", "GROUP BY/HAVING");
    auto joined = parse_select("SELECT a.id, b.name FROM a INNER JOIN b ON a.id = b.a_id WHERE b.name = 'Ada'");
    check(joined.join && joined.join->table == "b" && joined.join->left_column == "a.id", "JOIN parse");
    auto created = parse_statement("CREATE TABLE people (id INTEGER NOT NULL, name TEXT);");
    check(std::holds_alternative<CreateTableStatement>(created) &&
          std::get<CreateTableStatement>(created).columns.size() == 2 &&
          !std::get<CreateTableStatement>(created).columns[0].nullable, "CREATE TABLE parse");
    auto inserted = parse_statement("INSERT INTO people (name, id) VALUES ('Ada', -7)");
    check(std::holds_alternative<InsertStatement>(inserted) &&
          std::get<InsertStatement>(inserted).columns[0] == "name" &&
          std::get<std::int64_t>(std::get<InsertStatement>(inserted).values[1]) == -7,
          "INSERT parse");
    auto updated = parse_statement("UPDATE people SET name = 'Grace', score = NULL WHERE id = 8");
    check(std::holds_alternative<UpdateStatement>(updated) &&
          std::get<UpdateStatement>(updated).assignments.size() == 2 &&
          std::get<UpdateStatement>(updated).where->kind == Expression::Kind::Compare,
          "UPDATE parse");
    auto deleted = parse_statement("DELETE FROM people WHERE score IS NULL");
    check(std::holds_alternative<DeleteStatement>(deleted) &&
          std::get<DeleteStatement>(deleted).where->kind == Expression::Kind::IsNull,
          "DELETE parse");
    fails("SELECT a.id FROM a JOIN b ON a.id < b.id", "Expected = in JOIN condition");
    fails("SELECT SUM(*) FROM t", "Expected identifier");
    fails("SELECT COUNT(*) FROM t HAVING x > 1", "HAVING requires an aggregate");
    fails("", "line 1, column 1");
    fails("SELECT", "Expected identifier");
    fails("SELECT x FROM", "Expected identifier");
    fails("SELECT x FROM t WHERE x =", "Expected value");
    fails("SELECT x FROM t WHERE x IS", "Expected NULL");
    fails("SELECT x FROM t WHERE x IS NOT 1", "Expected NULL");
    fails("SELECT x FROM t WHERE (x = 1", "Expected )");
    fails("SELECT x FROM t LIMIT -1", "Expected nonnegative integer LIMIT");
    fails("SELECT x FROM t LIMIT 1.5", "Expected nonnegative integer LIMIT");
    fails("SELECT x FROM t; SELECT y FROM t", "Unexpected trailing token");
    fails("SELECT x FROM t GROUP BY x, y", "Unexpected trailing token");
    try { (void)parse_statement("CREATE TABLE t (x INTEGER,)"); check(false, "CREATE trailing comma"); }
    catch (const std::runtime_error&) {}
    try { (void)parse_statement("INSERT INTO t VALUES (1,)"); check(false, "INSERT trailing comma"); }
    catch (const std::runtime_error&) {}
    try { (void)parse_statement("UPDATE t SET x = WHERE y = 1"); check(false, "UPDATE missing value"); }
    catch (const std::runtime_error&) {}
    try { (void)parse_statement("DELETE t WHERE x = 1"); check(false, "DELETE missing FROM"); }
    catch (const std::runtime_error&) {}
    std::cout << "Parser tests passed.\n";
}
