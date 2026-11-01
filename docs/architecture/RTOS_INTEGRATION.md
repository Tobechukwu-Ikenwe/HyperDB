# RTOS and bare-metal integration

Status: design path only. No FreeRTOS, Zephyr, other RTOS, or bare-metal build or hardware test has passed. A Linux `single-threaded` preset validates the no-mutex code path, but does not validate an RTOS toolchain or adapter.

An RTOS task calls HyperDB directly. The engine creates no task or background thread. In the default single-threaded profile, one connection is confined to one caller at a time; the application is responsible for scheduling access. A shared connection may use application mutex callbacks only after a lock-order and priority-inversion policy is tested. A mutex callback may block; the API must document whether it can wait and whether the application may cancel the wait. No callback may reenter a handle while its lock is held. A bare-metal build must not pull in pthreads, Win32 threading, or an RTOS header through core code.

| Operation | Potential blocking today | Target contract |
| --- | --- | --- |
| Prepare | SQL tokenization, allocation, schema lookup | Bounded input and memory; no I/O unless catalog fetch is documented. |
| Step | Table scan, sort, join, grouping, device reads | Caller-driven bounded work unit or documented maximum work; explicit timeout/cancel policy where supported. |
| Commit | File/device writes and durable sync | May block until backend barrier completes; never performed by a hidden worker. |
| Checkpoint | Data writes and sync | Explicit application call with bounded work or incremental progress. |
| Close | Flush/release policy | Explicit status for any required flush; no silent destructor-only commit. |

The application supplies storage and memory callbacks with contexts that outlive the database. An optional synchronization adapter can map to FreeRTOS, Zephyr, or another RTOS; these adapters stay outside the shared core. If priority inheritance is required, the application-provided mutex must supply it. The engine must publish lock order before multi-connection tests. A fake adapter can test callback semantics but is not evidence of actual RTOS timing, interrupt safety, or flash durability. Calls from interrupts are unsupported until a specific adapter is designed and tested.

Representative acceptance: no-thread/no-filesystem core compile, a C embedding example linked with the target toolchain, allocator-failure and fake storage tests, task-level functional tests on one actual RTOS, measured stack and heap peaks, and power-cut tests on the chosen storage backend. For bare metal, the same checks apply without task synchronization; target-specific startup and C++ runtime support must be verified. No memory tier or hardware is marked supported solely by cross-compilation.
