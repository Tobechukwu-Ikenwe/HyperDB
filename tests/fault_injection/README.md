# Fault injection tests

Storage operation faults are currently exercised alongside their owning storage tests: see `tests/storage/disk_manager_tests.cpp`, `tests/storage/buffer_pool_tests.cpp`, and `src/storage/fault_file.cpp`. The tests cover selected read/write/sync failures and verify page allocation state. A dedicated power-loss/recovery suite will be added after transaction recovery exists; no crash-recovery guarantee is currently tested.
