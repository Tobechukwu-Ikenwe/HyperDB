#include "hyperdb/c_api.h"

#include <assert.h>
#include <string.h>

int main(void) {
    hdb_database *database = NULL;
    assert(hdb_database_create(&database) == HDB_OK);
    assert(database != NULL);

    char error[128] = {0};
    hdb_result *result = NULL;
    assert(hdb_execute(database, "CREATE TABLE people (id INTEGER, name TEXT)", &result, error,
                       sizeof(error)) == HDB_OK);
    assert(hdb_result_is_write(result) && hdb_result_modified(result));
    hdb_result_destroy(result);

    assert(hdb_execute(database, "INSERT INTO people VALUES (7, 'Ada')", &result, error,
                       sizeof(error)) == HDB_OK);
    assert(hdb_result_rows_affected(result) == 1);
    hdb_result_destroy(result);

    assert(hdb_execute(database, "SELECT id, name FROM people", &result, error, sizeof(error)) ==
           HDB_OK);
    assert(hdb_result_column_count(result) == 2);
    assert(hdb_result_row_count(result) == 1);
    const char *name = NULL;
    size_t name_size = 0;
    assert(hdb_result_column_name(result, 1, &name, &name_size) == HDB_OK);
    assert(name_size == 4 && memcmp(name, "name", name_size) == 0);
    hdb_value value;
    assert(hdb_result_value(result, 0, 0, &value) == HDB_OK);
    assert(value.type == HDB_INTEGER && value.data.integer_value == 7);
    assert(hdb_result_value(result, 0, 1, &value) == HDB_OK);
    assert(value.type == HDB_TEXT && value.data.text_value.size == 3);
    assert(memcmp(value.data.text_value.data, "Ada", 3) == 0);
    assert(hdb_result_value(result, 1, 0, &value) == HDB_INVALID_ARGUMENT);
    hdb_result_destroy(result);

    assert(hdb_execute(database, "SELECT missing FROM people", &result, error, sizeof(error)) ==
           HDB_SQL_ERROR);
    assert(result == NULL && error[0] != '\0');
    assert(hdb_execute(database, "SELECT 1", NULL, error, sizeof(error)) == HDB_INVALID_ARGUMENT);
    hdb_database_destroy(database);
    return 0;
}
