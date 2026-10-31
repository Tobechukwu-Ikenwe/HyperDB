# ADR 0010: One core with explicit platform capabilities

Status: accepted design, 2026-10-31; implementation pending.

## Context

The SQL engine and legacy snapshots are in one CMake library with a POSIX file adapter. The page path assumes byte-addressable files, 8192-byte pages, `std::mutex`, and a WAL durability callback that has no implementation. Splitting into platform-specific SQL engines would risk semantic drift. Claiming raw-flash support through the present file interface would hide erase and power-loss behavior.

## Alternatives considered

1. Keep POSIX-like file calls in the core and emulate them on every target. This is small initially, but imposes filesystem and process semantics on RTOS and bare metal.
2. Fork desktop and embedded engines. This reduces short-term adapter work but duplicates SQL, storage, and recovery logic.
3. Keep one engine and inject storage, allocation, and optional synchronization services. Require storage adapters to state geometry and durable-barrier capabilities. This preserves one behavior contract, but requires careful migration of current STL and snapshot APIs.

## Decision

Use option 3. The core accepts a logical random-access storage device. A POSIX file, Windows file, filesystem-backed embedded device, or application-supplied flash translation layer may implement it. A raw erase-before-write chip is not accepted until its adapter supplies logical overwrite, capacity, required alignment, and a verified sync barrier. The core validates capabilities at open and returns an explicit unsupported-configuration error. SQL grammar and semantics remain shared; limits produce explicit resource errors.

The first portable runtime mode is single-threaded, caller-driven, and has no background task. A connection is confined to its calling context unless an application synchronization adapter provides and tests stronger access. A stable C ABI uses opaque handles and callbacks; the C++ API remains a convenience layer over the same implementation. Existing `HDBSNAP1` files remain readable through a migration path while page storage is developed.

## Recovery choice and flash trade-off

The proposed first durable page protocol is single-writer, no-steal redo WAL on a logical device: uncommitted dirty pages cannot reach data storage; a checksummed commit record is synced before success; committed pages are replayed after interruption. This fits the existing pageLSN/cache seam but is **not implemented**. Rollback journaling and copy-on-write roots remain alternatives. WAL writes and checkpoints can amplify flash wear; a raw-flash adapter must provide wear leveling/FTL or the design must be revisited after measured device behavior. No raw-flash durability claim follows from this decision.

## Verification gates

Keep the Linux SQL tests green while splitting targets. Test the same storage contract against a memory fake and each platform adapter. Reject unsupported geometry. Add power-cut recovery tests before connecting SQL to the page path; run actual target validation before changing portability-matrix status. Each implementation slice gets its own regression tests and one-command exit gate.
