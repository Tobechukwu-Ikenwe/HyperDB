#include "storage/disk_manager.h"
#include <gtest/gtest.h>

#include <algorithm>
#include <cassert>
#include <memory>
#include <mutex>
#include <new>
#include <vector>

namespace {
class MemoryFile final : public hyperdb::File {
  public:
    explicit MemoryFile(std::shared_ptr<std::vector<std::byte>> bytes) : bytes_(std::move(bytes)) {}

    hyperdb::Status read_at(std::uint64_t offset, std::span<std::byte> output) noexcept override {
        std::lock_guard guard(mutex_);
        if (offset > bytes_->size() || output.size() > bytes_->size() - offset)
            return {hyperdb::ErrorCode::corruption, "Read outside memory device"};
        std::copy_n(bytes_->begin() + static_cast<std::ptrdiff_t>(offset), output.size(),
                    output.begin());
        return {};
    }

    hyperdb::Status write_at(std::uint64_t offset,
                             std::span<const std::byte> input) noexcept override {
        std::lock_guard guard(mutex_);
        if (offset > std::vector<std::byte>().max_size() ||
            input.size() > std::vector<std::byte>().max_size() - offset)
            return {hyperdb::ErrorCode::resource_exhausted, "Memory device is too large"};
        try {
            const auto end = static_cast<std::size_t>(offset) + input.size();
            if (end > bytes_->size())
                bytes_->resize(end);
            std::copy(input.begin(), input.end(),
                      bytes_->begin() + static_cast<std::ptrdiff_t>(offset));
            return {};
        } catch (const std::bad_alloc &) {
            return {hyperdb::ErrorCode::resource_exhausted, "Memory device allocation failed"};
        }
    }

    hyperdb::Status sync() noexcept override { return {}; }

    hyperdb::Result<std::uint64_t> size() noexcept override {
        std::lock_guard guard(mutex_);
        return static_cast<std::uint64_t>(bytes_->size());
    }

  private:
    std::shared_ptr<std::vector<std::byte>> bytes_;
    std::mutex mutex_;
};
} // namespace

TEST(DeviceStorageTests, ExistingCases) {
    using namespace hyperdb;
    auto backing = std::make_shared<std::vector<std::byte>>();
    auto opened = DiskManager::open(std::make_unique<MemoryFile>(backing), true);
    EXPECT_TRUE(opened.ok());
    auto manager = std::move(opened).value();
    const auto allocated = manager->allocate_slotted_page();
    EXPECT_TRUE(allocated.ok() && allocated.value() == 1);
    auto page = manager->read_page(1);
    EXPECT_TRUE(page.ok());
    const std::array<std::byte, 3> record{std::byte{0x10}, std::byte{0x20}, std::byte{0x30}};
    EXPECT_TRUE(insert_record(page.value(), record, 1).ok());
    EXPECT_TRUE(manager->write_page(1, page.value()).ok());
    manager.reset();

    opened = DiskManager::open(std::make_unique<MemoryFile>(backing), false);
    EXPECT_TRUE(opened.ok());
    manager = std::move(opened).value();
    EXPECT_TRUE(manager->page_count() == 2);
    const auto restored = manager->read_page(1);
    EXPECT_TRUE(restored.ok());
    const auto decoded = read_record(restored.value(), 0);
    EXPECT_TRUE(decoded.ok() && decoded.value().size() == record.size());
    EXPECT_TRUE(std::equal(decoded.value().begin(), decoded.value().end(), record.begin()));
    EXPECT_TRUE(!DiskManager::open(std::unique_ptr<File>{}, false).ok());
}
