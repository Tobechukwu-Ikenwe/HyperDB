#pragma once

#include <cctype>
#include <string>

// SQL identifiers in this subset use ASCII case-insensitive lookup.
inline std::string normalize_identifier(std::string name) {
    for (char& ch : name)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return name;
}
