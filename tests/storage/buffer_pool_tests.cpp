#include "storage/buffer_pool.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
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

int main() {
    using namespace hyperdb;
    const auto path = std::filesystem::temp_directory_path() /
                      ("hyperdb-buffer-pool-test-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    auto fs = make_posix_file_system();
    auto opened = DiskManager::open(*fs, path, true);
    assert(opened.ok());
    auto disk = std::move(opened).value();
    assert(disk->allocate_slotted_page().value() == 1);
    assert(disk->allocate_slotted_page().value() == 2);
    TestWal wal;
    assert(BufferPool::create(*disk, wal, 0).status().code() == ErrorCode::invalid_argument);
    auto created = BufferPool::create(*disk, wal, 1);
    assert(created.ok());
    auto pool = std::move(created).value();
    assert(pool->read(0).status().code() == ErrorCode::invalid_argument);
    auto first = pool->read(1).value();
    const std::array<std::byte, 2> record{std::byte{0xaa}, std::byte{0xbb}};
    assert(insert_record(first, record, 7).ok());
    assert(pool->write(1, first).ok());
    assert(pool->read(1).value() == first);
    wal.fail = true;
    assert(pool->read(2).status().code() == ErrorCode::io_error);
    assert(wal.calls.size() == 1 && wal.calls[0] == 7);
    assert(disk->read_page(1).value() != first);
    wal.fail = false;
    assert(pool->read(2).ok());
    assert(disk->read_page(1).value() == first);
    assert(pool->flush_all().ok());
    pool.reset();
    disk.reset();
    opened = DiskManager::open(*fs, path, false);
    assert(opened.ok());
    disk = std::move(opened).value();
    assert(disk->read_page(1).value() == first);
    disk.reset();
    std::filesystem::remove(path);
}
