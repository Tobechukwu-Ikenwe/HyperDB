#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

// SQLite-style storage classes.
using Value = std::variant<
    std::monostate,
    std::int64_t,
    double,
    std::string,
    std::vector<std::uint8_t>
>;
