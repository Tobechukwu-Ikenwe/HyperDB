# Storage engine format (in progress)

The legacy engine uses `HDBSNAP1` whole-file snapshots. The new page format is separate and is not yet connected to SQL execution. [ADR 0005](decisions/0005-m1-page-format.md) defines the version-1 header, common page trailer, and slotted-page layout byte by byte. Its checksum and bounds checks are covered by `hyperdb_page_format_tests` once that target is built.

Page zero is the file header. Pages 1 and above will hold slotted records, free-space metadata, or overflow data. The current codec supports slotted records, and the disk manager can allocate, validate, read, and write them. [ADR 0006](decisions/0006-m1-disk-manager.md) describes page allocation and orphan-tail handling. The file I/O boundary is described in [ADR 0003](decisions/0003-m1-file-io.md). SQL execution still uses legacy snapshots.
