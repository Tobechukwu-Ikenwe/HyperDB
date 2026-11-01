# ADR 0015: Support a single-threaded core profile

Status: Accepted, 2026-11-01.

## Context

The core used `std::mutex` in `DiskManager` and `BufferPool` even when an application controlled all calls from one thread. Some embedded C++ runtimes do not provide a mutex implementation, and single-threaded use is a required mode.

## Decision

Introduce a BasicLockable `hyperdb::Mutex` alias. The normal profile maps it to `std::mutex`; `HYPERDB_SINGLE_THREADED=ON` maps it to a no-op lock and exports the definition to consumers. The single-threaded contract requires that callers never access a shared database, disk manager, or buffer pool concurrently. Add a dedicated CMake preset and run the regression suite in this profile. The disk manager's multithread allocation test is excluded in this profile and its single-thread path remains tested.

## Trade-offs and limits

This removes mutex runtime use from the core in that profile, but does not remove atomics, exceptions, STL allocation, filesystem declarations, or OS calls from separately linked adapters. It is not a substitute for application synchronization. Misuse from multiple threads is a data race.

## Validation

Configure and build the `single-threaded` preset and run its CTest suite. Verify that the compiled core has no references to `pthread_mutex` or `std::mutex` lock/unlock functions. Keep the default threaded configuration and its concurrency tests unchanged.
