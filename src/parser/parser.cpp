#include "parser.h"

#include "lexer.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {

std::string upper(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return value;
}

std::string unquote(const std::string& text) {
    if (text.empty() || (text[0] != '\'' && text[0] != '"' && text[0] != '`' && text[0] != '[')) return text;
    const char delimiter = text[0] == '[' ? ']' : text[0];
    std::string result;
    for (std::size_t i = 1; i + 1 < text.size(); ++i) {
        result += text[i];
        if (text[i] == delimiter && i + 2 < text.size() && text[i + 1] == delimiter) ++i;
    }
    return result;
}

std::unique_ptr<Expression> node(Expression::Kind kind,
                                 std::unique_ptr<Expression> left = {},
                                 std::unique_ptr<Expression> right = {}) {
    auto result = std::make_unique<Expression>();
    result->kind = kind;
    result->left = std::move(left);
    result->right = std::move(right);
    return result;
}

class Parser {
public:
    Parser(std::string sql, std::vector<Token> tokens)
        : sql_(std::move(sql)), tokens_(std::move(tokens)) {}

    Statement statement() {
        if (accept_word("CREATE")) return create_table();
        if (accept_word("INSERT")) return insert();
        if (accept_word("UPDATE")) return update();
        if (accept_word("DELETE")) return delete_rows();
        return parse();
    }

    SelectStatement parse() {
        require_word("SELECT");
        SelectStatement result;
        if (accept(TokenType::Operator, "*")) {
            result.all_columns = true;
        } else {
            result.items.push_back(select_item());
            while (accept(TokenType::Comma)) result.items.push_back(select_item());
        }
        require_word("FROM");
        result.table = identifier();
        const bool inner = accept_word("INNER");
        if (inner || accept_word("JOIN")) {
            if (inner) require_word("JOIN");
            InnerJoin join;
            join.table = identifier();
            require_word("ON");
            join.left_column = identifier();
            if (!accept(TokenType::Operator, "=")) fail("Expected = in JOIN condition");
            join.right_column = identifier();
            result.join = std::move(join);
        }
        if (accept_word("WHERE")) result.where = parse_or();
        if (accept_word("GROUP")) {
            require_word("BY");
            result.group_by = identifier();
        }
        if (accept_word("HAVING")) {
            Having having;
            having.aggregate = select_item();
            if (having.aggregate.kind == SelectItem::Kind::Column)
                fail("HAVING requires an aggregate");
            having.op = comparison_operator();
            auto value = operand();
            if (value->kind != Expression::Kind::Literal)
                fail("HAVING requires a literal value");
            having.literal = std::move(value->literal);
            result.having = std::move(having);
        }
        if (accept_word("ORDER")) {
            require_word("BY");
            OrderBy order{identifier()};
            if (accept_word("DESC")) order.ascending = false;
            else accept_word("ASC");
            result.order_by = std::move(order);
        }
        if (accept_word("LIMIT")) {
            if (!current() || current()->type != TokenType::Number) fail("Expected nonnegative integer LIMIT");
            const auto token = *current();
            if (token.text.find_first_not_of("0123456789") != std::string::npos)
                fail("Expected nonnegative integer LIMIT");
            try {
                const auto value = std::stoull(token.text);
                if (value > std::numeric_limits<std::size_t>::max()) fail("LIMIT is too large");
                result.limit = static_cast<std::size_t>(value);
            } catch (const std::out_of_range&) { fail("LIMIT is too large"); }
            ++index_;
        }
        finish();
        return result;
    }

private:
    void finish() {
        accept(TokenType::Semicolon);
        if (current()) fail("Unexpected trailing token");
    }

    CreateTableStatement create_table() {
        require_word("TABLE");
        CreateTableStatement result;
        result.table = identifier();
        if (!accept(TokenType::Parenthesis, "(")) fail("Expected ( after table name");
        do {
            ColumnDefinition column;
            column.name = identifier();
            column.type = identifier();
            if (accept_word("NOT")) {
                require_word("NULL");
                column.nullable = false;
            }
            result.columns.push_back(std::move(column));
        } while (accept(TokenType::Comma));
        if (!accept(TokenType::Parenthesis, ")")) fail("Expected ) after columns");
        finish();
        return result;
    }

    InsertStatement insert() {
        require_word("INTO");
        InsertStatement result;
        result.table = identifier();
        if (accept(TokenType::Parenthesis, "(")) {
            do { result.columns.push_back(identifier()); }
            while (accept(TokenType::Comma));
            if (!accept(TokenType::Parenthesis, ")")) fail("Expected ) after column names");
        }
        require_word("VALUES");
        if (!accept(TokenType::Parenthesis, "(")) fail("Expected ( after VALUES");
        do {
            auto value = operand();
            if (value->kind != Expression::Kind::Literal)
                fail("INSERT VALUES requires literals");
            result.values.push_back(std::move(value->literal));
        } while (accept(TokenType::Comma));
        if (!accept(TokenType::Parenthesis, ")")) fail("Expected ) after values");
        finish();
        return result;
    }

    UpdateStatement update() {
        UpdateStatement result;
        result.table = identifier();
        require_word("SET");
        do {
            auto name = identifier();
            if (!accept(TokenType::Operator, "=")) fail("Expected = in SET assignment");
            auto value = operand();
            if (value->kind != Expression::Kind::Literal)
                fail("UPDATE SET requires literal values");
            result.assignments.emplace_back(std::move(name), std::move(value->literal));
        } while (accept(TokenType::Comma));
        if (accept_word("WHERE")) result.where = parse_or();
        finish();
        return result;
    }

    DeleteStatement delete_rows() {
        require_word("FROM");
        DeleteStatement result;
        result.table = identifier();
        if (accept_word("WHERE")) result.where = parse_or();
        finish();
        return result;
    }

    const Token* current() const { return index_ < tokens_.size() ? &tokens_[index_] : nullptr; }

    [[noreturn]] void fail(const std::string& message) const {
        int line = 1, column = 1;
        if (current()) { line = current()->line; column = current()->column; }
        else for (char c : sql_) { if (c == '\n') { ++line; column = 1; } else ++column; }
        throw std::runtime_error(message + " at line " + std::to_string(line) +
                                 ", column " + std::to_string(column));
    }

    bool accept(TokenType type, const std::string& text = {}) {
        if (current() && current()->type == type && (text.empty() || current()->text == text)) {
            ++index_; return true;
        }
        return false;
    }

    bool accept_word(const std::string& word) {
        if (current() && current()->type == TokenType::Keyword && upper(current()->text) == word) {
            ++index_; return true;
        }
        return false;
    }

    void require_word(const std::string& word) { if (!accept_word(word)) fail("Expected " + word); }

    std::string identifier() {
        if (!current() || current()->type != TokenType::Identifier) fail("Expected identifier");
        auto result = unquote(current()->text);
        if (result.empty()) fail("Empty identifier");
        ++index_;
        if (accept(TokenType::Dot)) {
            if (!current() || current()->type != TokenType::Identifier)
                fail("Expected identifier after .");
            result += "." + unquote(current()->text);
            ++index_;
        }
        return result;
    }

    SelectItem select_item() {
        static const std::pair<const char*, SelectItem::Kind> aggregates[] = {
            {"COUNT", SelectItem::Kind::Count}, {"SUM", SelectItem::Kind::Sum},
            {"AVG", SelectItem::Kind::Avg}, {"MIN", SelectItem::Kind::Min},
            {"MAX", SelectItem::Kind::Max}
        };
        for (const auto& [word, kind] : aggregates) {
            if (accept_word(word)) {
                if (!accept(TokenType::Parenthesis, "(")) fail("Expected ( after aggregate");
                SelectItem item{kind, {}};
                if (kind == SelectItem::Kind::Count && accept(TokenType::Operator, "*")) {
                    // COUNT(*) includes NULL rows.
                } else {
                    item.column = identifier();
                }
                if (!accept(TokenType::Parenthesis, ")")) fail("Expected ) after aggregate");
                return item;
            }
        }
        return {SelectItem::Kind::Column, identifier()};
    }

    std::string comparison_operator() {
        if (!current() || current()->type != TokenType::Operator) fail("Expected comparison operator");
        const auto op = current()->text;
        if (op != "=" && op != "==" && op != "!=" && op != "<>" &&
            op != "<" && op != "<=" && op != ">" && op != ">=") fail("Unsupported comparison operator");
        ++index_;
        return op;
    }

    std::unique_ptr<Expression> parse_or() {
        auto left = parse_and();
        while (accept_word("OR")) left = node(Expression::Kind::Or, std::move(left), parse_and());
        return left;
    }
    std::unique_ptr<Expression> parse_and() {
        auto left = parse_not();
        while (accept_word("AND")) left = node(Expression::Kind::And, std::move(left), parse_not());
        return left;
    }
    std::unique_ptr<Expression> parse_not() {
        if (accept_word("NOT")) return node(Expression::Kind::Not, parse_not());
        if (accept(TokenType::Parenthesis, "(")) {
            auto expr = parse_or();
            if (!accept(TokenType::Parenthesis, ")")) fail("Expected )");
            return expr;
        }
        auto left = operand();
        if (accept_word("IS")) {
            const bool negated = accept_word("NOT");
            require_word("NULL");
            return node(negated ? Expression::Kind::IsNotNull : Expression::Kind::IsNull,
                        std::move(left));
        }
        const auto op = comparison_operator();
        auto expr = node(Expression::Kind::Compare, std::move(left), operand());
        expr->op = op;
        return expr;
    }
    std::unique_ptr<Expression> operand() {
        bool negative = accept(TokenType::Operator, "-");
        if (!current()) fail("Expected value");
        if (current()->type == TokenType::Number) {
            auto expr = node(Expression::Kind::Literal);
            const auto text = current()->text;
            try {
                if (text.find_first_of(".eE") != std::string::npos) expr->literal = std::stod((negative ? "-" : "") + text);
                else expr->literal = std::stoll((negative ? "-" : "") + text);
            } catch (const std::exception&) { fail("Invalid numeric value"); }
            ++index_; return expr;
        }
        if (negative) fail("Expected number after -");
        if (current()->type == TokenType::String) {
            auto expr = node(Expression::Kind::Literal);
            expr->literal = unquote(current()->text);
            ++index_; return expr;
        }
        if (accept_word("NULL")) { auto expr = node(Expression::Kind::Literal); expr->literal = std::monostate{}; return expr; }
        auto expr = node(Expression::Kind::Column);
        expr->name = identifier();
        return expr;
    }

    std::string sql_;
    std::vector<Token> tokens_;
    std::size_t index_ = 0;
};
} // namespace

SelectStatement parse_select(const std::string& sql) {
    return Parser(sql, tokenize(sql)).parse();
}

Statement parse_statement(const std::string& sql) {
    return Parser(sql, tokenize(sql)).statement();
}
