#include "storage/disk_manager.h"

#include <limits>
#include <new>

namespace hyperdb {
namespace {

bool valid_page_offset(std::uint64_t page_id) noexcept {
    return page_id <= std::numeric_limits<std::uint64_t>::max() / kPageSize;
}

std::uint64_t page_offset(std::uint64_t page_id) noexcept { return page_id * kPageSize; }

} // namespace

Result<std::unique_ptr<DiskManager>> DiskManager::open(FileSystem &file_system,
                                                       const std::filesystem::path &path,
                                                       bool create) noexcept {
    auto opened = file_system.open(path, create);
    if (!opened.ok()) {
        return opened.status();
    }
    auto file = std::move(opened).value();
    const auto file_size = file->size();
    if (!file_size.ok()) {
        return file_size.status();
    }

    Page header{};
    std::uint64_t count = 1;
    if (file_size.value() == 0 && create) {
        if (const auto initialized = init_file_header(header); !initialized.ok()) {
            return initialized;
        }
        if (const auto written = file->write_at(0, header); !written.ok()) {
            return written;
        }
        if (const auto synced = file->sync(); !synced.ok()) {
            return synced;
        }
    } else {
        if (file_size.value() < kPageSize) {
            return Status{ErrorCode::corruption, "Page file is shorter than its header"};
        }
        if (const auto read = file->read_at(0, header); !read.ok()) {
            return read;
        }
        const auto parsed = parse_file_header(header);
        if (!parsed.ok()) {
            return parsed.status();
        }
        count = parsed.value().page_count;
        if (!valid_page_offset(count) || file_size.value() < page_offset(count)) {
            return Status{ErrorCode::corruption, "Page file is shorter than its recorded count"};
        }
    }

    auto manager = std::unique_ptr<DiskManager>(new (std::nothrow)
                                                    DiskManager(std::move(file), header, count));
    if (!manager) {
        return Status{ErrorCode::resource_exhausted, "Disk manager allocation failed"};
    }
    return manager;
}

Result<Page> DiskManager::read_page(std::uint64_t page_id) noexcept {
    if (page_id == 0) {
        return Status{ErrorCode::invalid_argument, "Page zero is file metadata"};
    }
    if (page_id >= page_count()) {
        return Status{ErrorCode::not_found, "Page ID is outside the file"};
    }
    Page page{};
    if (const auto read = file_->read_at(page_offset(page_id), page); !read.ok()) {
        return read;
    }
    const auto parsed = parse_slotted_page(page);
    if (!parsed.ok()) {
        return parsed.status();
    }
    if (parsed.value().page_id != page_id) {
        return Status{ErrorCode::corruption, "Stored page ID does not match offset"};
    }
    return page;
}

Status DiskManager::write_page(std::uint64_t page_id, const Page &page) noexcept {
    if (page_id == 0) {
        return {ErrorCode::invalid_argument, "Page zero is file metadata"};
    }
    if (page_id >= page_count()) {
        return {ErrorCode::not_found, "Page ID is outside the file"};
    }
    const auto proposed = parse_slotted_page(page);
    if (!proposed.ok()) {
        return proposed.status();
    }
    if (proposed.value().page_id != page_id) {
        return {ErrorCode::invalid_argument, "Page ID does not match write offset"};
    }
    const auto current = read_page(page_id);
    if (!current.ok()) {
        return current.status();
    }
    const auto old_header = parse_slotted_page(current.value());
    if (proposed.value().page_lsn < old_header.value().page_lsn) {
        return {ErrorCode::invalid_argument, "PageLSN cannot decrease"};
    }
    return file_->write_at(page_offset(page_id), page);
}

Result<std::uint64_t> DiskManager::allocate_slotted_page() noexcept {
    std::lock_guard guard(allocation_mutex_);
    if (allocation_poisoned_) {
        return Status{ErrorCode::io_error,
                      "Allocation state requires reopen after header I/O failure"};
    }
    const auto new_page_id = page_count();
    if (!valid_page_offset(new_page_id + 1)) {
        return Status{ErrorCode::resource_exhausted, "Page ID exceeds supported file offset"};
    }
    Page page{};
    if (const auto initialized = init_slotted_page(page, new_page_id); !initialized.ok()) {
        return initialized;
    }
    if (const auto written = file_->write_at(page_offset(new_page_id), page); !written.ok()) {
        return written;
    }
    if (const auto synced = file_->sync(); !synced.ok()) {
        return synced;
    }
    auto next_header = header_;
    const auto old_header = parse_file_header(header_);
    if (const auto updated =
            set_page_count(next_header, new_page_id + 1, old_header.value().page_lsn);
        !updated.ok()) {
        return updated;
    }
    if (const auto written = file_->write_at(0, next_header); !written.ok()) {
        allocation_poisoned_ = true;
        return written;
    }
    if (const auto synced = file_->sync(); !synced.ok()) {
        allocation_poisoned_ = true;
        return synced;
    }
    header_ = next_header;
    page_count_.store(new_page_id + 1, std::memory_order_release);
    return new_page_id;
}

} // namespace hyperdb
