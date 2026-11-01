#include "hyperdb/c_api.h"

#include "QueryExecutor/query_executor.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <variant>

struct hdb_database {
    Database database;
};

struct hdb_result {
    StatementResult result;
};

namespace {
void set_error(char *buffer, size_t capacity, const char *message) noexcept {
    if (!buffer || capacity == 0)
        return;
    const auto length = std::min(capacity - 1, std::strlen(message));
    std::memcpy(buffer, message, length);
    buffer[length] = '\0';
}

hdb_status exception_status(const std::exception &error, char *buffer, size_t capacity) noexcept {
    set_error(buffer, capacity, error.what());
    return HDB_SQL_ERROR;
}
} // namespace

extern "C" hdb_status hdb_database_create(hdb_database **out_database) {
    if (!out_database)
        return HDB_INVALID_ARGUMENT;
    *out_database = nullptr;
    try {
        *out_database = new hdb_database{};
        return HDB_OK;
    } catch (const std::bad_alloc &) {
        return HDB_OUT_OF_MEMORY;
    } catch (...) {
        return HDB_INTERNAL_ERROR;
    }
}

extern "C" void hdb_database_destroy(hdb_database *database) { delete database; }

extern "C" hdb_status hdb_execute(hdb_database *database, const char *sql, hdb_result **out_result,
                                  char *error_buffer, size_t error_buffer_size) {
    if (out_result)
        *out_result = nullptr;
    if (!database || !sql || !out_result) {
        set_error(error_buffer, error_buffer_size, "Invalid argument");
        return HDB_INVALID_ARGUMENT;
    }
    try {
        auto result = std::unique_ptr<hdb_result>(new hdb_result{});
        result->result = execute_statement(database->database, sql);
        *out_result = result.release();
        set_error(error_buffer, error_buffer_size, "");
        return HDB_OK;
    } catch (const std::bad_alloc &) {
        set_error(error_buffer, error_buffer_size, "Out of memory");
        return HDB_OUT_OF_MEMORY;
    } catch (const std::exception &error) {
        return exception_status(error, error_buffer, error_buffer_size);
    } catch (...) {
        set_error(error_buffer, error_buffer_size, "Unknown internal error");
        return HDB_INTERNAL_ERROR;
    }
}

extern "C" void hdb_result_destroy(hdb_result *result) { delete result; }
extern "C" int hdb_result_is_write(const hdb_result *result) {
    return result && result->result.is_write;
}
extern "C" int hdb_result_modified(const hdb_result *result) {
    return result && result->result.modified;
}
extern "C" size_t hdb_result_rows_affected(const hdb_result *result) {
    return result ? result->result.rows_affected : 0;
}
extern "C" size_t hdb_result_column_count(const hdb_result *result) {
    return result ? result->result.query.columns.size() : 0;
}
extern "C" size_t hdb_result_row_count(const hdb_result *result) {
    return result ? result->result.query.rows.size() : 0;
}

extern "C" hdb_status hdb_result_column_name(const hdb_result *result, size_t column,
                                             const char **out_name, size_t *out_size) {
    if (!result || !out_name || !out_size)
        return HDB_INVALID_ARGUMENT;
    if (column >= result->result.query.columns.size())
        return HDB_INVALID_ARGUMENT;
    const auto &name = result->result.query.columns[column];
    *out_name = name.data();
    *out_size = name.size();
    return HDB_OK;
}

extern "C" hdb_status hdb_result_value(const hdb_result *result, size_t row, size_t column,
                                       hdb_value *out_value) {
    if (!result || !out_value || row >= result->result.query.rows.size() ||
        column >= result->result.query.columns.size() ||
        column >= result->result.query.rows[row].size())
        return HDB_INVALID_ARGUMENT;
    const auto &value = result->result.query.rows[row][column];
    if (std::holds_alternative<std::monostate>(value)) {
        out_value->type = HDB_NULL;
    } else if (const auto *integer = std::get_if<std::int64_t>(&value)) {
        out_value->type = HDB_INTEGER;
        out_value->data.integer_value = *integer;
    } else if (const auto *real = std::get_if<double>(&value)) {
        out_value->type = HDB_REAL;
        out_value->data.real_value = *real;
    } else if (const auto *text = std::get_if<std::string>(&value)) {
        out_value->type = HDB_TEXT;
        out_value->data.text_value = {text->data(), text->size()};
    } else {
        const auto &blob = std::get<std::vector<std::uint8_t>>(value);
        out_value->type = HDB_BLOB;
        out_value->data.blob_value = {blob.data(), blob.size()};
    }
    return HDB_OK;
}
