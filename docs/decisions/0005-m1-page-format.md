# ADR 0005: Versioned 8 KB page format

Status: accepted, 2026-10-28.

## Context

The legacy `HDBSNAP1` snapshot has no page structure or checksum. M1 needs a distinct, little-endian format that can be checked before a disk manager trusts bytes. Page zero holds file metadata; pages one and above can hold slotted rows. WAL integration will later control pageLSN changes.

## Options

Use a checksum in each page header or a fixed trailer. A trailer makes the payload boundary explicit and leaves page-header fields contiguous. CRC32C can be implemented in portable C++20 without a runtime dependency; hardware acceleration can be measured later.

## Decision

Use 8,192-byte pages and CRC32C over bytes `0..8187`, storing the checksum as little-endian `u32` at `8188..8191`. Page zero starts with `HDBPAGE1` (8 bytes), followed by format version `u32` at 8, page size `u32` at 12, page count `u64` at 16, and pageLSN `u64` at 24. Format version starts at 1. Page count includes page zero and is at least one.

Slotted pages have a 32-byte header: type `u16` at 0, flags `u16` at 2, page ID `u64` at 4, pageLSN `u64` at 12, lower free-space boundary `u16` at 20, upper boundary `u16` at 22, slot count `u16` at 24, and six reserved zero bytes at 26. Slots are `(offset u16, length u16)` pairs beginning at byte 32. Record payloads grow backward from byte 8188. All fields are little-endian. Page IDs begin at one. Insertions are append-only within one page in this step; deletion, slot reuse, free-space map, and overflow pages follow in later M1 steps.

Every decoder verifies exact page size, checksum, known version/type, bounds, and non-overlapping live record spans before exposing data. A mutation updates pageLSN and CRC32C before returning. A pageLSN supplied by a caller must not decrease. The disk manager will enforce WAL-before-data before any such page reaches disk.

## Tests and exit criterion for this step

Test the standard CRC32C vector, golden little-endian header bytes, file-header and slotted-page round trips, corrupted checksum, malformed bounds and overlapping slots with corrected CRC, exact-fit insertion, and too-large insertion. Reproduce with `cmake --build --preset debug && ctest --preset debug --output-on-failure`.

## Consequences

The new magic cannot be confused with `HDBSNAP1`. CRC32C detects accidental corruption but is not authentication. The file format is still incomplete until free-space and overflow structures are specified and tested.
