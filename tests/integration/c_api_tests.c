#include "hyperdb/c_api.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct allocator_ledger {
    size_t allocations;
    size_t deallocations;
} allocator_ledger;

static void *tracked_allocate(void *context, size_t bytes, size_t alignment) {
    allocator_ledger *ledger = (allocator_ledger *)context;
    if (alignment < sizeof(void *))
        alignment = sizeof(void *);
    if ((alignment & (alignment - 1)) != 0 || bytes > SIZE_MAX - alignment)
        return NULL;
    size_t rounded = (bytes + alignment - 1) & ~(alignment - 1);
    if (rounded == 0)
        rounded = alignment;
    void *pointer = aligned_alloc(alignment, rounded);
    if (pointer)
        ++ledger->allocations;
    return pointer;
}

static void tracked_deallocate(void *context, void *pointer, size_t bytes, size_t alignment) {
    (void)bytes;
    (void)alignment;
    if (!pointer)
        return;
    allocator_ledger *ledger = (allocator_ledger *)context;
    ++ledger->deallocations;
    free(pointer);
}

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

    allocator_ledger ledger = {0, 0};
    hdb_allocator_config config = HDB_ALLOCATOR_CONFIG_INIT;
    config.kind = HDB_ALLOCATOR_APPLICATION;
    config.budget_bytes = 1024 * 1024;
    config.context = &ledger;
    config.allocate = tracked_allocate;
    config.deallocate = tracked_deallocate;
    assert(hdb_database_create_with_allocator(&config, &database) == HDB_OK);
    assert(hdb_execute(database, "CREATE TABLE routed (id INTEGER)", &result, error,
                       sizeof(error)) == HDB_OK);
    hdb_result_destroy(result);
    hdb_allocator_stats stats = {0};
    assert(hdb_database_get_allocator_stats(database, &stats) == HDB_OK);
    assert(stats.allocation_count > 0 && stats.current_bytes > 0);
    assert(stats.has_remaining_budget && stats.remaining_budget < config.budget_bytes);
    hdb_database_destroy(database);
    assert(ledger.allocations > 0 && ledger.allocations == ledger.deallocations);

    config.budget_bytes = 1;
    assert(hdb_database_create_with_allocator(&config, &database) == HDB_OK);
    result = NULL;
    assert(hdb_execute(database, "CREATE TABLE limited (id INTEGER)", &result, error,
                       sizeof(error)) == HDB_OUT_OF_MEMORY);
    assert(result == NULL);
    assert(hdb_database_get_allocator_stats(database, &stats) == HDB_OK);
    assert(stats.failure_count == 1 && stats.current_bytes == 0);
    hdb_database_destroy(database);

    config.kind = HDB_ALLOCATOR_HYPERMALLOC;
    config.budget_bytes = 0;
    hdb_status hypermalloc_status = hdb_database_create_with_allocator(&config, &database);
    if (hypermalloc_status == HDB_OK) {
        assert(hdb_execute(database, "CREATE TABLE hm (id INTEGER)", &result, error,
                           sizeof(error)) == HDB_OK);
        hdb_result_destroy(result);
        hdb_database_destroy(database);
    } else {
        assert(hypermalloc_status == HDB_UNSUPPORTED);
    }
    return 0;
}
