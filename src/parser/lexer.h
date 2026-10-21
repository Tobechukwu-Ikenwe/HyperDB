#pragma once

#include <string>
#include <vector>

enum class TokenType {
    Keyword,
    Identifier,
    Semicolon,
    Operator,
    Number,
    String,
    Comma,
    Parenthesis,
    Dot
};

struct Token {
    TokenType type;
    std::string text; // Exact spelling from the input, including quote marks.
    int line;         // One-based source position of the first character.
    int column;       // One-based byte column.
};

// Throws std::runtime_error with a source position for malformed input.
std::vector<Token> tokenize(const std::string& sql);
