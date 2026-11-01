#ifndef HYPERDB_C_API_H
#define HYPERDB_C_API_H

#include <stddef.h>
#include <stdint.h>

#define HYPERDB_C_API_VERSION 1

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hdb_database hdb_database;
typedef struct hdb_result hdb_result;

typedef enum hdb_status {
    HDB_OK = 0,
    HDB_INVALID_ARGUMENT = 1,
    HDB_OUT_OF_MEMORY = 2,
    HDB_SQL_ERROR = 3,
    HDB_INTERNAL_ERROR = 4
} hdb_status;

typedef enum hdb_value_type {
    HDB_NULL = 0,
    HDB_INTEGER = 1,
    HDB_REAL = 2,
    HDB_TEXT = 3,
    HDB_BLOB = 4
} hdb_value_type;

typedef struct hdb_value {
    hdb_value_type type;
    union {
        int64_t integer_value;
        double real_value;
        struct {
            const char *data;
            size_t size;
        } text_value;
        struct {
            const uint8_t *data;
            size_t size;
        } blob_value;
    } data;
} hdb_value;

/* Creates an empty, in-memory database. Each handle is single-threaded. */
hdb_status hdb_database_create(hdb_database **out_database);
void hdb_database_destroy(hdb_database *database);

/* Executes one SQL statement. Destroy *out_result with hdb_result_destroy. */
hdb_status hdb_execute(hdb_database *database, const char *sql,
                       hdb_result **out_result, char *error_buffer,
                       size_t error_buffer_size);

void hdb_result_destroy(hdb_result *result);
int hdb_result_is_write(const hdb_result *result);
int hdb_result_modified(const hdb_result *result);
size_t hdb_result_rows_affected(const hdb_result *result);
size_t hdb_result_column_count(const hdb_result *result);
size_t hdb_result_row_count(const hdb_result *result);

/* Column-name and text/blob data pointers are borrowed from result. */
hdb_status hdb_result_column_name(const hdb_result *result, size_t column,
                                  const char **out_name, size_t *out_size);
hdb_status hdb_result_value(const hdb_result *result, size_t row, size_t column,
                            hdb_value *out_value);

#ifdef __cplusplus
}
#endif

#endif
