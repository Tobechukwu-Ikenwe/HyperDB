#pragma once

#include "hyperdb/file_system.h"
#include "storage/page_format.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>

namespace hyperdb {

/** Owns a page file and maps page IDs to fixed offsets. */
class DiskManager {
  public:
    /** Opens an existing page file, or creates one when create is true. */
    [[nodiscard]] static Result<std::unique_ptr<DiskManager>>
    open(FileSystem &file_system, const std::filesystem::path &path, bool create) noexcept;

    [[nodiscard]] std::uint64_t page_count() const noexcept {
        return page_count_.load(std::memory_order_acquire);
    }

    /** Reads and validates a slotted page. Page zero is not exposed here. */
    [[nodiscard]] Result<Page> read_page(std::uint64_t page_id) noexcept;

    /** Writes one validated existing page. The caller decides when to sync. */
    [[nodiscard]] Status write_page(std::uint64_t page_id, const Page &page) noexcept;

    /** Creates a checksummed empty page, syncs it, then publishes its ID in page zero. */
    [[nodiscard]] Result<std::uint64_t> allocate_slotted_page() noexcept;

    [[nodiscard]] Status sync() noexcept { return file_->sync(); }

  private:
    DiskManager(std::unique_ptr<File> file, Page header, std::uint64_t page_count)
        : file_(std::move(file)), header_(header), page_count_(page_count) {}

    std::unique_ptr<File> file_;
    Page header_;
    std::atomic<std::uint64_t> page_count_;
    std::mutex allocation_mutex_;
    bool allocation_poisoned_ = false;
};

} // namespace hyperdb
