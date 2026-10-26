#include "database/database.h"
#include "parser/parser.h"
#include "QueryExecutor/query_executor.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::uint32_t read_u32() {
    unsigned char bytes[4];
    if (!std::cin.read(reinterpret_cast<char*>(bytes), 4)) throw std::runtime_error("Truncated bridge input");
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

std::uint64_t read_u64() {
    const auto low = read_u32(), high = read_u32();
    return static_cast<std::uint64_t>(low) | (static_cast<std::uint64_t>(high) << 32);
}

std::string read_bytes(std::uint32_t limit = 16 * 1024 * 1024) {
    const auto length = read_u32();
    if (length > limit) throw std::runtime_error("Bridge field is too large");
    std::string value(length, '\0');
    if (!std::cin.read(value.data(), length)) throw std::runtime_error("Truncated bridge input");
    return value;
}

Value read_value() {
    const auto kind = std::cin.get();
    if (kind == EOF) throw std::runtime_error("Truncated bridge input");
    if (kind == 0) return std::monostate{};
    if (kind == 1) {
        const auto bits = read_u64();
        std::int64_t value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    if (kind == 2) {
        const auto bits = read_u64();
        double value;
        std::memcpy(&value, &bits, sizeof(value));
        if (!std::isfinite(value)) throw std::runtime_error("Non-finite SQLite number");
        return value;
    }
    if (kind == 3) return read_bytes();
    if (kind == 4) {
        const auto bytes = read_bytes();
        return std::vector<std::uint8_t>(bytes.begin(), bytes.end());
    }
    throw std::runtime_error("Unknown bridge value type");
}

void json_string(const std::string& value) {
    static const char* hex = "0123456789abcdef";
    std::cout << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') std::cout << '\\' << c;
        else if (c < 0x20) std::cout << "\\u00" << hex[c >> 4] << hex[c & 15];
        else std::cout << c;
    }
    std::cout << '"';
}

void json_value(const Value& value) {
    if (std::holds_alternative<std::monostate>(value)) std::cout << "null";
    else if (std::holds_alternative<std::int64_t>(value)) std::cout << std::get<std::int64_t>(value);
    else if (std::holds_alternative<double>(value)) {
        const auto number = std::get<double>(value);
        if (!std::isfinite(number)) throw std::runtime_error("Non-finite result number");
        std::cout.precision(std::numeric_limits<double>::max_digits10);
        std::cout << number;
    } else if (std::holds_alternative<std::string>(value)) json_string(std::get<std::string>(value));
    else {
        static const char* hex = "0123456789abcdef";
        std::string bytes = "0x";
        for (auto byte : std::get<std::vector<std::uint8_t>>(value)) {
            bytes += hex[byte >> 4]; bytes += hex[byte & 15];
        }
        json_string(bytes);
    }
}

Database load_database(const SelectStatement& statement) {
    char magic[4];
    if (!std::cin.read(magic, 4) || std::memcmp(magic, "HDB1", 4) != 0)
        throw std::runtime_error("Invalid bridge input header");
    const auto count = read_u32();
    if (count < 1 || count > 2) throw std::runtime_error("Expected one or two tables");
    Database database;
    for (std::uint32_t t = 0; t < count; ++t) {
        const auto name = read_bytes(1024);
        const auto columns = read_u32();
        const auto rows = read_u32();
        if (columns == 0 || columns > 512 || rows > 10000)
            throw std::runtime_error("Bridge table exceeds limits");
        Table table;
        for (std::uint32_t c = 0; c < columns; ++c)
            table.schema.add_column({read_bytes(1024), "", ""});
        table.rows.reserve(rows);
        for (std::uint32_t r = 0; r < rows; ++r) {
            Row row;
            row.reserve(columns);
            for (std::uint32_t c = 0; c < columns; ++c) row.push_back(read_value());
            table.rows.push_back(std::move(row));
        }
        if (!database.create_table(name, table)) throw std::runtime_error("Duplicate bridge table");
    }
    if (!database.find_table(statement.table) ||
        (statement.join && !database.find_table(statement.join->table)))
        throw std::runtime_error("Bridge input is missing a query table");
    if (statement.join &&
        database.find_table(statement.table)->rows.size() *
        database.find_table(statement.join->table)->rows.size() > 1000000)
        throw std::runtime_error("Bridge join exceeds the 1,000,000-pair limit");
    return database;
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::runtime_error("Usage: hyperdb_bridge --tables|--execute SQL");
        const auto statement = parse_select(argv[2]);
        const std::string mode = argv[1];
        if (mode == "--tables") {
            std::cout << '[';
            json_string(statement.table);
            if (statement.join) { std::cout << ','; json_string(statement.join->table); }
            std::cout << "]\n";
            return 0;
        }
        if (mode != "--execute") throw std::runtime_error("Unknown bridge mode");
        const auto database = load_database(statement);
        const auto result = execute_query(database, statement);
        std::cout << "{\"columns\":[";
        for (std::size_t i = 0; i < result.columns.size(); ++i) {
            if (i) std::cout << ',';
            json_string(result.columns[i]);
        }
        std::cout << "],\"rows\":[";
        for (std::size_t r = 0; r < result.rows.size(); ++r) {
            if (r) std::cout << ',';
            std::cout << '[';
            for (std::size_t c = 0; c < result.rows[r].size(); ++c) {
                if (c) std::cout << ',';
                json_value(result.rows[r][c]);
            }
            std::cout << ']';
        }
        std::cout << "]}\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
