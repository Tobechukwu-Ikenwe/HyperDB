#include "parser/lexer.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void expect(const std::string& sql, const std::vector<Token>& expected) {
    const auto actual = tokenize(sql);
    check(actual.size() == expected.size(), "Wrong token count for: " + sql);
    for (std::size_t i = 0; i < actual.size(); ++i) {
        check(actual[i].type == expected[i].type &&
              actual[i].text == expected[i].text &&
              actual[i].line == expected[i].line &&
              actual[i].column == expected[i].column,
              "Wrong token at index " + std::to_string(i) + " for: " + sql);
    }
}

void expect_error(const std::string& sql, const std::string& message) {
    try {
        (void)tokenize(sql);
    } catch (const std::runtime_error& error) {
        check(std::string(error.what()) == message, "Wrong error for: " + sql);
        return;
    }
    check(false, "Expected error for: " + sql);
}

} // namespace

int main() {
    using T = TokenType;
    expect("", {});
    expect(" \t\r\n ", {});
    expect("SeLeCt a.b, COUNT(*) FROM t;", {
        {T::Keyword, "SeLeCt", 1, 1}, {T::Identifier, "a", 1, 8},
        {T::Dot, ".", 1, 9}, {T::Identifier, "b", 1, 10},
        {T::Comma, ",", 1, 11}, {T::Keyword, "COUNT", 1, 13},
        {T::Parenthesis, "(", 1, 18}, {T::Operator, "*", 1, 19},
        {T::Parenthesis, ")", 1, 20}, {T::Keyword, "FROM", 1, 22},
        {T::Identifier, "t", 1, 27}, {T::Semicolon, ";", 1, 28}
    });
    expect("SELECT x\r\n-- comment\r\nFROM /* outer\n /* inner */ end */ t", {
        {T::Keyword, "SELECT", 1, 1}, {T::Identifier, "x", 1, 8},
        {T::Keyword, "FROM", 3, 1}, {T::Identifier, "t", 4, 21}
    });
    expect("'it''s -- /* fine' \"select\" `a``b` [a]]b]", {
        {T::String, "'it''s -- /* fine'", 1, 1},
        {T::Identifier, "\"select\"", 1, 20},
        {T::Identifier, "`a``b`", 1, 29},
        {T::Identifier, "[a]]b]", 1, 36}
    });
    expect("0 12.34 .5 6. 7e2 8.9E-3 1e+4", {
        {T::Number, "0", 1, 1}, {T::Number, "12.34", 1, 3},
        {T::Number, ".5", 1, 9}, {T::Number, "6.", 1, 12},
        {T::Number, "7e2", 1, 15}, {T::Number, "8.9E-3", 1, 19},
        {T::Number, "1e+4", 1, 26}
    });
    expect("a<=b AND c<>d OR e!=f || g==h >= i", {
        {T::Identifier, "a", 1, 1}, {T::Operator, "<=", 1, 2},
        {T::Identifier, "b", 1, 4}, {T::Keyword, "AND", 1, 6},
        {T::Identifier, "c", 1, 10}, {T::Operator, "<>", 1, 11},
        {T::Identifier, "d", 1, 13}, {T::Keyword, "OR", 1, 15},
        {T::Identifier, "e", 1, 18}, {T::Operator, "!=", 1, 19},
        {T::Identifier, "f", 1, 21}, {T::Operator, "||", 1, 23},
        {T::Identifier, "g", 1, 26}, {T::Operator, "==", 1, 27},
        {T::Identifier, "h", 1, 29}, {T::Operator, ">=", 1, 31},
        {T::Identifier, "i", 1, 34}
    });
    expect("abc_123--tail", {{T::Identifier, "abc_123", 1, 1}});
    expect("'first\nsecond'\rnext", {
        {T::String, "'first\nsecond'", 1, 1}, {T::Identifier, "next", 3, 1}
    });
    expect_error("'unfinished", "Unterminated string literal at line 1, column 1");
    expect_error("x /* missing", "Unterminated block comment at line 1, column 3");
    expect_error("\"missing", "Unterminated quoted identifier at line 1, column 1");
    expect_error("1e+", "Invalid numeric exponent at line 1, column 1");
    expect_error("12abc", "Invalid numeric literal at line 1, column 1");
    expect_error("a ! b", "Unsupported character at line 1, column 3");
    expect_error("a @ b", "Unsupported character at line 1, column 3");
    std::cout << "Lexer tests passed.\n";
}
