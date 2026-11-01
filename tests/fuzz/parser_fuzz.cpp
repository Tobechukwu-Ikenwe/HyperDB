#include "parser/lexer.h"
#include "parser/parser.h"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string sql(reinterpret_cast<const char*>(data), size);
    try {
        (void)tokenize(sql);
        (void)parse_statement(sql);
    } catch (const std::exception&) {
        // Syntax errors are expected for arbitrary bytes.
    }
    return 0;
}
