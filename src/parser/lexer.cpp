#include "lexer.h"

#include <cctype>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

namespace {

bool letter(char ch) {
    return std::isalpha(static_cast<unsigned char>(ch)) != 0 || ch == '_';
}

bool digit(char ch) {
    return std::isdigit(static_cast<unsigned char>(ch)) != 0;
}

bool identifier_part(char ch) {
    return letter(ch) || digit(ch);
}

bool keyword(std::string_view word) {
    static const std::unordered_set<std::string> words = {
        "ALL", "AND", "AS", "ASC", "AVG", "BETWEEN", "BY", "CASE",
        "COUNT", "CREATE", "CROSS", "DELETE", "DESC", "DISTINCT", "DROP",
        "ELSE", "END", "EXCEPT", "EXISTS", "FALSE", "FROM", "FULL", "GROUP",
        "HAVING", "IN", "INNER", "INSERT", "INTERSECT", "INTO", "IS", "JOIN",
        "LEFT", "LIKE", "LIMIT", "MAX", "MIN", "NOT", "NULL", "OFFSET", "ON",
        "OR", "ORDER", "OUTER", "RECURSIVE", "RIGHT", "SELECT", "SET", "SUM",
        "TABLE", "THEN", "TRUE", "UNION", "UPDATE", "USING", "VALUES", "WHEN",
        "WHERE", "WITH"
    };
    std::string upper;
    upper.reserve(word.size());
    for (unsigned char ch : word) {
        upper.push_back(static_cast<char>(std::toupper(ch)));
    }
    return words.find(upper) != words.end();
}

class Scanner {
public:
    explicit Scanner(const std::string& input) : input_(input) {}

    std::vector<Token> scan() {
        while (!done()) {
            const char ch = peek();
            if (std::isspace(static_cast<unsigned char>(ch))) {
                advance();
            } else if (starts("--")) {
                line_comment();
            } else if (starts("/*")) {
                block_comment();
            } else if (letter(ch)) {
                identifier();
            } else if (digit(ch) || (ch == '.' && digit(peek(1)))) {
                number();
            } else if (ch == '\'') {
                quoted('\'', TokenType::String);
            } else if (ch == '"' || ch == '`') {
                quoted(ch, TokenType::Identifier);
            } else if (ch == '[') {
                quoted(']', TokenType::Identifier);
            } else if (ch == ';') {
                single(TokenType::Semicolon);
            } else if (ch == ',') {
                single(TokenType::Comma);
            } else if (ch == '(' || ch == ')') {
                single(TokenType::Parenthesis);
            } else if (ch == '.') {
                single(TokenType::Dot);
            } else {
                operation();
            }
        }
        return tokens_;
    }

private:
    bool done() const { return index_ == input_.size(); }

    char peek(std::size_t offset = 0) const {
        return offset < input_.size() - index_ ? input_[index_ + offset] : '\0';
    }

    bool starts(std::string_view value) const {
        return input_.compare(index_, value.size(), value) == 0;
    }

    void advance() {
        if (peek() == '\r') {
            ++index_;
            if (!done() && peek() == '\n') ++index_;
            ++line_;
            column_ = 1;
        } else if (peek() == '\n') {
            ++index_;
            ++line_;
            column_ = 1;
        } else {
            ++index_;
            ++column_;
        }
    }

    void consume(std::size_t count) {
        while (count--) advance();
    }

    void emit(TokenType type, std::size_t start, int line, int column) {
        tokens_.push_back({type, input_.substr(start, index_ - start), line, column});
    }

    [[noreturn]] void error(const std::string& message, int line, int column) const {
        throw std::runtime_error(message + " at line " + std::to_string(line) +
                                 ", column " + std::to_string(column));
    }

    void single(TokenType type) {
        const auto start = index_;
        const auto line = line_, column = column_;
        advance();
        emit(type, start, line, column);
    }

    void identifier() {
        const auto start = index_;
        const auto line = line_, column = column_;
        do { advance(); } while (!done() && identifier_part(peek()));
        emit(keyword(std::string_view(input_).substr(start, index_ - start))
                 ? TokenType::Keyword : TokenType::Identifier, start, line, column);
    }

    void number() {
        const auto start = index_;
        const auto line = line_, column = column_;
        while (digit(peek())) advance();
        if (peek() == '.' && (digit(peek(1)) || index_ > start)) {
            advance();
            while (digit(peek())) advance();
        }
        if (peek() == 'e' || peek() == 'E') {
            advance();
            if (peek() == '+' || peek() == '-') advance();
            if (!digit(peek())) error("Invalid numeric exponent", line, column);
            while (digit(peek())) advance();
        }
        if (letter(peek())) error("Invalid numeric literal", line, column);
        emit(TokenType::Number, start, line, column);
    }

    void quoted(char delimiter, TokenType type) {
        const auto start = index_;
        const auto line = line_, column = column_;
        advance();
        while (!done()) {
            if (peek() == delimiter) {
                advance();
                if (peek() == delimiter) {
                    advance(); // A doubled delimiter is an escaped delimiter.
                } else {
                    emit(type, start, line, column);
                    return;
                }
            } else {
                advance();
            }
        }
        error(type == TokenType::String ? "Unterminated string literal"
                                        : "Unterminated quoted identifier", line, column);
    }

    void line_comment() {
        consume(2);
        while (!done() && peek() != '\r' && peek() != '\n') advance();
    }

    void block_comment() {
        const auto line = line_, column = column_;
        consume(2);
        int depth = 1;
        while (!done()) {
            if (starts("/*")) {
                consume(2);
                ++depth;
            } else if (starts("*/")) {
                consume(2);
                if (--depth == 0) return;
            } else {
                advance();
            }
        }
        error("Unterminated block comment", line, column);
    }

    void operation() {
        static constexpr std::string_view two_character[] = {
            "<=", ">=", "!=", "<>", "==", "||"
        };
        const auto start = index_;
        const auto line = line_, column = column_;
        for (const auto op : two_character) {
            if (starts(op)) {
                consume(2);
                emit(TokenType::Operator, start, line, column);
                return;
            }
        }
        if (std::string_view("=<>*+-/%").find(peek()) != std::string_view::npos) {
            advance();
            emit(TokenType::Operator, start, line, column);
            return;
        }
        error("Unsupported character", line, column);
    }

    const std::string& input_;
    std::size_t index_ = 0;
    int line_ = 1;
    int column_ = 1;
    std::vector<Token> tokens_;
};

} // namespace

std::vector<Token> tokenize(const std::string& sql) {
    return Scanner(sql).scan();
}
