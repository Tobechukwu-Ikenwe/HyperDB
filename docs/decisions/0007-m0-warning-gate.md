# ADR 0007: Warning gate for the full engine core

Status: accepted, 2026-10-28.

## Context

M0 requires strict compiler warnings across legacy and new core code. A Debug build of every target passes `-Wall -Wextra -Wpedantic -Werror` with GCC 15.2.0. GCC 15.2.0 at `-O3` reports `-Wfree-nonheap-object` while inlining `std::variant` copy assignment in the legacy query executor. The diagnostic points to libstdc++'s allocator and variant implementation, without a user-code deallocation site. The existing ASan and UBSan test suites have not detected an invalid free.

## Decision

Apply `-Wall -Wextra -Wpedantic -Werror` to the entire `hyperdb_core` target in every build. For GCC 15 and newer Release builds, suppress only `-Wfree-nonheap-object` until the compiler/library diagnostic can be resolved or ruled out more conclusively. Keep the source-level warning flags active for every other diagnostic. Build and test Debug and Release after this change, and keep GCC, Clang, and macOS builds in CI.

## Exit criterion

`cmake --build --preset debug && ctest --preset debug --output-on-failure` and `cmake --build --preset release && ctest --preset release --output-on-failure` must pass. M0 also requires a verified remote CI run; local results alone do not close it.

## Consequences

GCC 15 Release has one narrowly scoped diagnostic exemption. HDB-6 remains open for compiler-version comparison and source-level investigation.
