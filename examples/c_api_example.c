#include "hyperdb/c_api.h"

#include <stdio.h>

int main(void) {
    hdb_database *database = NULL;
    if (hdb_database_create(&database) != HDB_OK)
        return 1;

    char error[160];
    hdb_result *result = NULL;
    const char *statements[] = {
        "CREATE TABLE greeting (message TEXT)",
        "INSERT INTO greeting VALUES ('Hello from C')",
        "SELECT message FROM greeting"
    };
    for (size_t i = 0; i < sizeof(statements) / sizeof(statements[0]); ++i) {
        const hdb_status status = hdb_execute(database, statements[i], &result,
                                               error, sizeof(error));
        if (status != HDB_OK) {
            fprintf(stderr, "HyperDB error: %s\n", error);
            hdb_database_destroy(database);
            return 1;
        }
        if (i == 2) {
            hdb_value value;
            if (hdb_result_value(result, 0, 0, &value) == HDB_OK &&
                value.type == HDB_TEXT)
                printf("%.*s\n", (int)value.data.text_value.size,
                       value.data.text_value.data);
        }
        hdb_result_destroy(result);
        result = NULL;
    }

    hdb_database_destroy(database);
    return 0;
}
