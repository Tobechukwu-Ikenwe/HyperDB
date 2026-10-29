#include "storage/buffer_pool.h"

#include <new>

namespace hyperdb {

Result<std::unique_ptr<BufferPool>> BufferPool::create(DiskManager &disk, WalDurability &wal,
                                                       std::size_t capacity) noexcept {
    if (capacity == 0) {
        return Status{ErrorCode::invalid_argument, "Buffer pool capacity must be positive"};
    }
    try {
        std::vector<Frame> frames(capacity);
        auto pool = std::unique_ptr<BufferPool>(new (std::nothrow)
                                                    BufferPool(disk, wal, std::move(frames)));
        if (!pool) {
            return Status{ErrorCode::resource_exhausted, "Buffer pool allocation failed"};
        }
        return pool;
    } catch (const std::bad_alloc &) {
        return Status{ErrorCode::resource_exhausted, "Buffer pool allocation failed"};
    }
}

Status BufferPool::flush_frame(Frame &frame) noexcept {
    if (!frame.dirty)
        return {};
    const auto parsed = parse_slotted_page(frame.page);
    if (!parsed.ok())
        return parsed.status();
    if (const auto durable = wal_.flush_through(parsed.value().page_lsn); !durable.ok()) {
        return durable;
    }
    if (const auto written = disk_.write_page(frame.page_id, frame.page); !written.ok()) {
        return written;
    }
    if (const auto synced = disk_.sync(); !synced.ok())
        return synced;
    frame.dirty = false;
    return {};
}

Result<std::size_t> BufferPool::find_or_load(std::uint64_t page_id) noexcept {
    if (page_id == 0) {
        return Status{ErrorCode::invalid_argument, "Page zero is file metadata"};
    }
    for (std::size_t index = 0; index < frames_.size(); ++index) {
        if (frames_[index].page_id == page_id)
            return index;
    }
    std::size_t index = frames_.size();
    for (std::size_t candidate = 0; candidate < frames_.size(); ++candidate) {
        if (frames_[candidate].page_id == 0) {
            index = candidate;
            break;
        }
    }
    if (index == frames_.size()) {
        index = next_victim_;
        if (const auto flushed = flush_frame(frames_[index]); !flushed.ok())
            return flushed;
    }
    const auto loaded = disk_.read_page(page_id);
    if (!loaded.ok())
        return loaded.status();
    frames_[index].page = loaded.value();
    frames_[index].page_id = page_id;
    frames_[index].dirty = false;
    next_victim_ = (index + 1) % frames_.size();
    return index;
}

Result<Page> BufferPool::read(std::uint64_t page_id) noexcept {
    std::lock_guard guard(mutex_);
    const auto found = find_or_load(page_id);
    if (!found.ok())
        return found.status();
    return frames_[found.value()].page;
}

Status BufferPool::write(std::uint64_t page_id, const Page &page) noexcept {
    const auto proposed = parse_slotted_page(page);
    if (!proposed.ok())
        return proposed.status();
    if (proposed.value().page_id != page_id) {
        return {ErrorCode::invalid_argument, "Page ID does not match write target"};
    }
    std::lock_guard guard(mutex_);
    const auto found = find_or_load(page_id);
    if (!found.ok())
        return found.status();
    auto &frame = frames_[found.value()];
    const auto current = parse_slotted_page(frame.page);
    if (proposed.value().page_lsn < current.value().page_lsn) {
        return {ErrorCode::invalid_argument, "PageLSN cannot decrease"};
    }
    frame.page = page;
    frame.dirty = true;
    return {};
}

Status BufferPool::flush(std::uint64_t page_id) noexcept {
    std::lock_guard guard(mutex_);
    for (auto &frame : frames_) {
        if (frame.page_id == page_id)
            return flush_frame(frame);
    }
    return {ErrorCode::not_found, "Page is not cached"};
}

Status BufferPool::flush_all() noexcept {
    std::lock_guard guard(mutex_);
    for (auto &frame : frames_) {
        if (const auto flushed = flush_frame(frame); !flushed.ok())
            return flushed;
    }
    return {};
}

} // namespace hyperdb
