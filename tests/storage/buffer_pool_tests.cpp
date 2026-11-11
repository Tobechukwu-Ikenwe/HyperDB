#include "storage/buffer_pool.h"
#include <gtest/gtest.h>

#include <array>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <vector>

namespace {

class TestWal final : public hyperdb::WalDurability {
  public:
    hyperdb::Status flush_through(std::uint64_t lsn) noexcept override {
        calls.push_back(lsn);
        if (fail)
            return {hyperdb::ErrorCode::io_error, "Injected WAL failure"};
        return {};
    }
    std::vector<std::uint64_t> calls;
    bool fail = false;
};

} // namespace

TEST(BufferPoolTests, ExistingCases) {
    using namespace hyperdb;
    const auto path = std::filesystem::temp_directory_path() /
                      ("hyperdb-buffer-pool-test-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    auto fs = make_posix_file_system();
    auto opened = DiskManager::open(*fs, path, true);
    EXPECT_TRUE(opened.ok());
    auto disk = std::move(opened).value();
    EXPECT_TRUE(disk->allocate_slotted_page().value() == 1);
    EXPECT_TRUE(disk->allocate_slotted_page().value() == 2);
    TestWal wal;
    EXPECT_TRUE(BufferPool::create(*disk, wal, 0).status().code() == ErrorCode::invalid_argument);
    auto created = BufferPool::create(*disk, wal, 1);
    EXPECT_TRUE(created.ok());
    auto pool = std::move(created).value();
    EXPECT_TRUE(pool->read(0).status().code() == ErrorCode::invalid_argument);
    auto first = pool->read(1).value();
    const std::array<std::byte, 2> record{std::byte{0xaa}, std::byte{0xbb}};
    EXPECT_TRUE(insert_record(first, record, 7).ok());
    EXPECT_TRUE(pool->write(1, first).ok());
    EXPECT_TRUE(pool->read(1).value() == first);
    wal.fail = true;
    EXPECT_TRUE(pool->read(2).status().code() == ErrorCode::io_error);
    EXPECT_TRUE(wal.calls.size() == 1 && wal.calls[0] == 7);
    EXPECT_TRUE(disk->read_page(1).value() != first);
    wal.fail = false;
    EXPECT_TRUE(pool->read(2).ok());
    EXPECT_TRUE(disk->read_page(1).value() == first);
    EXPECT_TRUE(pool->flush_all().ok());
    pool.reset();
    disk.reset();
    opened = DiskManager::open(*fs, path, false);
    EXPECT_TRUE(opened.ok());
    disk = std::move(opened).value();
    EXPECT_TRUE(disk->read_page(1).value() == first);
    disk.reset();
    std::filesystem::remove(path);
}
