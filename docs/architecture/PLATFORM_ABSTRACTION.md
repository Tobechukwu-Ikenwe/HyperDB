# Platform abstraction contract

Status: target interface contract; no implementation or target support claim. See [ADR 0010](../decisions/0010-portable-core-contract.md).

## Logical storage

The engine will receive a caller-owned storage context with callbacks for `read_exact(offset, bytes)`, `write_exact(offset, bytes)`, `sync()`, `size()`, and capability discovery. Each callback reports a non-allocating status code. Partial transfers are errors with an explicit completed-byte count; the core may retry only when the contract marks retry safe. `sync` must be a durable ordering barrier for all prior successful writes, or the adapter must report that durable commits are unsupported. Capacity, read/program alignment, minimum atomic-write unit, erase-block size, erase-before-write requirement, and required write granularity are reported separately. An adapter must not claim random overwrite if it cannot provide it.

Logical pages are independent of physical erase blocks. The core validates geometry at open, aligns I/O where needed, and refuses unsupported combinations. Filesystem adapters own path opening, file creation, atomic replacement where used, and directory durability. Application-supplied device adapters own their driver lifetime and any wear-leveling or flash translation layer. Raw NAND bad-block handling is outside the initial core contract; an adapter must make it transparent or reject the target.

## Memory

An application allocator supplies `allocate(context, size, alignment)` and `deallocate(context, pointer, size, alignment)` or a fixed arena. The engine never frees caller memory through a different allocator. Limits include total engine bytes, page cache bytes, statement bytes, temporary query bytes, SQL length/depth, and active handles. Failures return resource status without corrupting committed data. The design must be validated with deterministic fail-after-N tests before claiming this behavior.

## Synchronization and clock

The default profile is single-threaded and invokes no mutex or task service. Optional application mutex callbacks are scoped to connection/pager boundaries and have documented lock order, wait behavior, and priority-inversion implications. A shared connection is not thread safe by default. No background worker thread is required. A monotonic clock callback is needed only for timeouts/measurement when those features are implemented; a wall clock is not required by current SQL semantics. Platform adapters own any timestamp naming or filesystem housekeeping.

## Error and lifetime rules

Callbacks must not throw across the C boundary. Callback contexts must outlive all handles using them. An adapter's `sync` guarantee is part of the database durability contract and is tested with fault injection and target-specific restart/power-cut tests. A fake in-memory adapter is a correctness oracle, not proof of flash or RTOS behavior.
