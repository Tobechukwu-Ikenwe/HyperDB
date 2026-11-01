#pragma once

#include "hyperdb/synchronization.h"
#include "storage/disk_manager.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace hyperdb {

/** Ensures WAL records through the supplied LSN are durable. */
class WalDurability {
  public:
    virtual ~WalDurability() = default;
    [[nodiscard]] virtual Status flush_through(std::uint64_t page_lsn) noexcept = 0;
};

/** Bounded single-user page cache. Dirty pages obey WAL-before-data on flush. */
class BufferPool {
  public:
    [[nodiscard]] static Result<std::unique_ptr<BufferPool>>
    create(DiskManager &disk, WalDurability &wal, std::size_t capacity) noexcept;

    [[nodiscard]] Result<Page> read(std::uint64_t page_id) noexcept;
    [[nodiscard]] Status write(std::uint64_t page_id, const Page &page) noexcept;
    [[nodiscard]] Status flush(std::uint64_t page_id) noexcept;
    [[nodiscard]] Status flush_all() noexcept;

  private:
    struct Frame {
        Page page{};
        std::uint64_t page_id = 0;
        bool dirty = false;
    };

    BufferPool(DiskManager &disk, WalDurability &wal, std::vector<Frame> frames)
        : disk_(disk), wal_(wal), frames_(std::move(frames)) {}

    [[nodiscard]] Result<std::size_t> find_or_load(std::uint64_t page_id) noexcept;
    [[nodiscard]] Status flush_frame(Frame &frame) noexcept;

    DiskManager &disk_;
    WalDurability &wal_;
    std::vector<Frame> frames_;
    std::size_t next_victim_ = 0;
    Mutex mutex_;
};

} // namespace hyperdb
