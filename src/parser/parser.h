#pragma once

#include "ast.h"

#include <string>

// Parses exactly one SELECT statement, with an optional terminal semicolon.
// Throws std::runtime_error with a one-based source location on syntax errors.
SelectStatement parse_select(const std::string& sql);

// Parses one supported SELECT, CREATE TABLE, INSERT, UPDATE, or DELETE statement.
Statement parse_statement(const std::string& sql);
