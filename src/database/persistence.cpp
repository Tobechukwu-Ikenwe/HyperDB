#include "database.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <system_error>
#ifdef __unix__
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {
constexpr std::string_view magic = "HDBSNAP1";
constexpr std::uint64_t max_bytes = 16 * 1024 * 1024;
constexpr std::uint64_t max_tables = 10000;
constexpr std::uint64_t max_columns = 512;
constexpr std::uint64_t max_rows = 1000000;
constexpr std::uint64_t max_snapshot_bytes = 1024ULL * 1024 * 1024;
constexpr std::uint64_t max_dimensions = 100000;

void write_raw(std::ostream& out, const void* bytes, std::size_t count) {
    out.write(static_cast<const char*>(bytes), static_cast<std::streamsize>(count));
    if (!out) throw std::runtime_error("Snapshot write failed");
}
void read_raw(std::istream& in, void* bytes, std::size_t count) {
    if (!in.read(static_cast<char*>(bytes), static_cast<std::streamsize>(count)))
        throw std::runtime_error("Truncated or unreadable HyperDB snapshot");
}
void write_uint(std::ostream& out, std::uint64_t number, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) {
        const auto byte = static_cast<unsigned char>(number & 0xff);
        write_raw(out, &byte, 1);
        number >>= 8;
    }
}
std::uint64_t read_uint(std::istream& in, unsigned bytes) {
    std::uint64_t number = 0;
    for (unsigned i = 0; i < bytes; ++i) {
        unsigned char byte;
        read_raw(in, &byte, 1);
        number |= static_cast<std::uint64_t>(byte) << (8 * i);
    }
    return number;
}
void write_string(std::ostream& out, const std::string& value) {
    if (value.size() > max_bytes) throw std::runtime_error("Snapshot field too large");
    write_uint(out, value.size(), 4);
    write_raw(out, value.data(), value.size());
}
std::string read_string(std::istream& in) {
    const auto length = read_uint(in, 4);
    if (length > max_bytes) throw std::runtime_error("Snapshot field too large");
    std::string value(static_cast<std::size_t>(length), '\0');
    read_raw(in, value.data(), value.size());
    return value;
}
void write_double(std::ostream& out, double number) {
    std::uint64_t bits;
    static_assert(sizeof(bits) == sizeof(number));
    std::memcpy(&bits, &number, sizeof(bits));
    write_uint(out, bits, 8);
}
double read_double(std::istream& in) {
    const auto bits = read_uint(in, 8);
    double number;
    std::memcpy(&number, &bits, sizeof(number));
    return number;
}
void write_value(std::ostream& out, const Value& value) {
    write_uint(out, value.index(), 1);
    if (const auto number = std::get_if<std::int64_t>(&value)) {
        std::uint64_t bits;
        std::memcpy(&bits, number, sizeof(bits));
        write_uint(out, bits, 8);
    } else if (const auto number = std::get_if<double>(&value)) {
        write_double(out, *number);
    } else if (const auto string = std::get_if<std::string>(&value)) {
        write_string(out, *string);
    } else if (const auto blob = std::get_if<std::vector<std::uint8_t>>(&value)) {
        if (blob->size() > max_bytes) throw std::runtime_error("Snapshot blob too large");
        write_uint(out, blob->size(), 4);
        write_raw(out, blob->data(), blob->size());
    }
}
Value read_value(std::istream& in) {
    switch (read_uint(in, 1)) {
    case 0: return std::monostate{};
    case 1: {
        const auto bits = read_uint(in, 8);
        std::int64_t number;
        std::memcpy(&number, &bits, sizeof(number));
        return number;
    }
    case 2: return read_double(in);
    case 3: return read_string(in);
    case 4: {
        const auto length = read_uint(in, 4);
        if (length > max_bytes) throw std::runtime_error("Snapshot blob too large");
        std::vector<std::uint8_t> blob(static_cast<std::size_t>(length));
        read_raw(in, blob.data(), blob.size());
        return blob;
    }
    default: throw std::runtime_error("Invalid snapshot value tag");
    }
}
bool read_bool(std::istream& in) {
    const auto value = read_uint(in, 1);
    if (value > 1) throw std::runtime_error("Invalid snapshot boolean");
    return value == 1;
}
void checked_count(std::uint64_t count, std::uint64_t maximum) {
    if (count > maximum) throw std::runtime_error("Snapshot count exceeds supported limit");
}

void sync_path(const std::filesystem::path& path, bool directory) {
#ifdef __unix__
    int flags = O_RDONLY;
#ifdef O_DIRECTORY
    if (directory) flags |= O_DIRECTORY;
#else
    (void)directory;
#endif
    const int fd = ::open(path.c_str(), flags);
    if (fd < 0) throw std::system_error(errno, std::generic_category(), "Open snapshot for sync");
    if (::fsync(fd) != 0) {
        const int error = errno;
        ::close(fd);
        throw std::system_error(error, std::generic_category(), "Sync snapshot");
    }
    if (::close(fd) != 0)
        throw std::system_error(errno, std::generic_category(), "Close synced snapshot");
#else
    (void)path;
    (void)directory;
#endif
}
} // namespace

void Database::save(const std::filesystem::path& path) const {
    if (path.empty()) throw std::invalid_argument("Snapshot path cannot be empty");
    const auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    auto temporary = path;
    temporary += ".tmp-" + suffix;
    try {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("Cannot open snapshot for writing");
        write_raw(out, magic.data(), magic.size());
        checked_count(tables.size(), max_tables);
        write_uint(out, tables.size(), 4);
        std::vector<std::string> names;
        names.reserve(tables.size());
        for (const auto& [name, table] : tables) names.push_back(name);
        std::sort(names.begin(), names.end());
        for (const auto& name : names) {
            const auto& table = tables.at(name);
            write_string(out, name);
            checked_count(table.schema.size(), max_columns);
            write_uint(out, table.schema.size(), 4);
            for (const auto& column : table.schema.get_columns()) {
                write_string(out, column.name);
                write_string(out, column.declared_type);
                write_string(out, column.affinity);
                write_uint(out, column.nullable, 1);
                write_uint(out, column.primary_key, 1);
                write_uint(out, column.auto_increment, 1);
                write_value(out, column.default_value);
            }
            checked_count(table.rows.size(), max_rows);
            write_uint(out, table.rows.size(), 8);
            for (const auto& row : table.rows) {
                if (row.size() != table.schema.size()) throw std::runtime_error("Invalid row width during snapshot");
                for (const auto& value : row) write_value(out, value);
            }
            std::vector<std::string> columns;
            if (const auto found = indexes.find(name); found != indexes.end()) {
                for (const auto& [column, index] : found->second) columns.push_back(column);
            }
            std::sort(columns.begin(), columns.end());
            write_uint(out, columns.size(), 4);
            for (const auto& column : columns) write_string(out, column);
            const auto vector = vector_indexes.find(name);
            write_uint(out, vector != vector_indexes.end(), 1);
            if (vector != vector_indexes.end()) {
                const auto& index = vector->second;
                checked_count(index.dimensions(), max_dimensions);
                write_uint(out, index.dimensions(), 4);
                write_uint(out, index.size(), 8);
                std::vector<RowID> ids;
                ids.reserve(index.size());
                for (const auto& [id, embedding] : index.entries()) ids.push_back(id);
                std::sort(ids.begin(), ids.end());
                for (const auto id : ids) {
                    if (id >= table.rows.size()) throw std::runtime_error("Invalid embedding row ID during snapshot");
                    write_uint(out, id, 8);
                    for (const auto value : index.entries().at(id)) write_double(out, value);
                }
            }
        }
        if (out.tellp() < 0 || static_cast<std::uint64_t>(out.tellp()) > max_snapshot_bytes)
            throw std::runtime_error("Snapshot exceeds supported file size");
        out.flush();
        if (!out) throw std::runtime_error("Snapshot flush failed");
        out.close();
        if (!out) throw std::runtime_error("Snapshot close failed");
        sync_path(temporary, false);
        std::filesystem::rename(temporary, path);
        sync_path(path.parent_path().empty() ? std::filesystem::path{"."} : path.parent_path(), true);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}

Database Database::load(const std::filesystem::path& path) {
    if (std::filesystem::file_size(path) > max_snapshot_bytes)
        throw std::runtime_error("Snapshot exceeds supported file size");
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open HyperDB snapshot");
    std::array<char, 8> signature{};
    read_raw(in, signature.data(), signature.size());
    if (std::string_view(signature.data(), signature.size()) != magic)
        throw std::runtime_error("Unsupported HyperDB snapshot format");
    Database database;
    const auto table_count = read_uint(in, 4);
    checked_count(table_count, max_tables);
    for (std::uint64_t t = 0; t < table_count; ++t) {
        const auto name = read_string(in);
        Table table;
        const auto column_count = read_uint(in, 4);
        if (column_count == 0) throw std::runtime_error("Snapshot table has no columns");
        checked_count(column_count, max_columns);
        for (std::uint64_t c = 0; c < column_count; ++c) {
            Column column;
            column.name = read_string(in);
            column.declared_type = read_string(in);
            column.affinity = read_string(in);
            column.nullable = read_bool(in);
            column.primary_key = read_bool(in);
            column.auto_increment = read_bool(in);
            column.default_value = read_value(in);
            table.schema.add_column(column);
        }
        const auto row_count = read_uint(in, 8);
        checked_count(row_count, max_rows);
        for (std::uint64_t r = 0; r < row_count; ++r) {
            Row row;
            row.reserve(static_cast<std::size_t>(column_count));
            for (std::uint64_t c = 0; c < column_count; ++c) row.push_back(read_value(in));
            table.rows.push_back(std::move(row));
        }
        if (!database.create_table(name, table)) throw std::runtime_error("Duplicate snapshot table");
        const auto index_count = read_uint(in, 4);
        checked_count(index_count, column_count);
        for (std::uint64_t i = 0; i < index_count; ++i) {
            if (!database.create_index(name, read_string(in)))
                throw std::runtime_error("Invalid or duplicate snapshot index");
        }
        if (read_bool(in)) {
            const auto dimensions = read_uint(in, 4);
            if (dimensions == 0) throw std::runtime_error("Invalid snapshot embedding dimensions");
            checked_count(dimensions, max_dimensions);
            database.create_vector_index(name, static_cast<std::size_t>(dimensions));
            const auto count = read_uint(in, 8);
            checked_count(count, row_count);
            for (std::uint64_t i = 0; i < count; ++i) {
                const auto id = read_uint(in, 8);
                if (id >= row_count) throw std::runtime_error("Invalid snapshot embedding row ID");
                std::vector<double> embedding(static_cast<std::size_t>(dimensions));
                for (auto& value : embedding) value = read_double(in);
                if (!database.set_embedding(name, static_cast<RowID>(id), std::move(embedding)))
                    throw std::runtime_error("Invalid snapshot embedding");
            }
            if (database.vector_indexes.at(name).size() != count)
                throw std::runtime_error("Duplicate snapshot embedding row ID");
        }
    }
    if (in.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("Trailing bytes in HyperDB snapshot");
    if (in.bad()) throw std::runtime_error("Snapshot read failed");
    return database;
}
