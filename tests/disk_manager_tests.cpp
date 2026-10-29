#include "storage/disk_manager.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <thread>
#include <vector>

namespace {

class FailingFileSystem final : public hyperdb::FileSystem {
  public:
    explicit FailingFileSystem(std::uint64_t fail_operation) : fail_operation_(fail_operation) {}

    hyperdb::Result<std::unique_ptr<hyperdb::File>> open(const std::filesystem::path &path,
                                                         bool create) noexcept override {
        auto opened = delegate_->open(path, create);
        if (!opened.ok())
            return opened.status();
        return hyperdb::make_fault_file(std::move(opened).value(), fail_operation_);
    }

  private:
    std::unique_ptr<hyperdb::FileSystem> delegate_ = hyperdb::make_posix_file_system();
    std::uint64_t fail_operation_;
};

} // namespace

int main() {
    using namespace hyperdb;
    const auto path = std::filesystem::temp_directory_path() /
                      ("hyperdb-disk-manager-test-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    auto fs = make_posix_file_system();
    assert(!DiskManager::open(*fs, path, false).ok());
    auto opened = DiskManager::open(*fs, path, true);
    assert(opened.ok());
    auto disk = std::move(opened).value();
    assert(disk->page_count() == 1);
    assert(disk->read_page(0).status().code() == ErrorCode::invalid_argument);
    assert(disk->allocate_slotted_page().value() == 1);
    assert(disk->page_count() == 2);
    auto page = disk->read_page(1).value();
    const std::array<std::byte, 2> record{std::byte{0xaa}, std::byte{0xbb}};
    assert(insert_record(page, record, 1).value() == 0);
    assert(disk->write_page(1, page).ok());
    assert(disk->sync().ok());
    assert(disk->read_page(2).status().code() == ErrorCode::not_found);
    assert(disk->write_page(0, page).code() == ErrorCode::invalid_argument);
    auto wrong_id = page;
    assert(init_slotted_page(wrong_id, 7).ok());
    assert(disk->write_page(1, wrong_id).code() == ErrorCode::invalid_argument);
    assert(init_slotted_page(wrong_id, 1, 0).ok());
    assert(disk->write_page(1, wrong_id).code() == ErrorCode::invalid_argument);
    disk.reset();

    auto reopened = DiskManager::open(*fs, path, false);
    assert(reopened.ok());
    disk = std::move(reopened).value();
    assert(disk->page_count() == 2);
    assert(disk->read_page(1).value() == page);
    disk.reset();

    auto raw = fs->open(path, false);
    assert(raw.ok());
    auto file = std::move(raw).value();
    assert(init_slotted_page(wrong_id, 7).ok());
    assert(file->write_at(kPageSize, wrong_id).ok());
    assert(file->sync().ok());
    reopened = DiskManager::open(*fs, path, false);
    assert(reopened.ok());
    disk = std::move(reopened).value();
    assert(disk->read_page(1).status().code() == ErrorCode::corruption);
    disk.reset();
    assert(file->write_at(kPageSize, page).ok());
    page[100] ^= std::byte{1};
    assert(file->write_at(kPageSize, page).ok());
    assert(file->sync().ok());
    file.reset();
    reopened = DiskManager::open(*fs, path, false);
    assert(reopened.ok());
    disk = std::move(reopened).value();
    assert(disk->read_page(1).status().code() == ErrorCode::corruption);
    disk.reset();

    raw = fs->open(path, false);
    assert(raw.ok());
    file = std::move(raw).value();
    const std::array<std::byte, 1> bad_magic{std::byte{0}};
    assert(file->write_at(0, bad_magic).ok());
    assert(file->sync().ok());
    file.reset();
    assert(DiskManager::open(*fs, path, false).status().code() == ErrorCode::corruption);

    const auto fault_path = path.string() + "-fault";
    opened = DiskManager::open(*fs, fault_path, true);
    assert(opened.ok());
    opened.value().reset();
    FailingFileSystem failing(4); // Open: size, header read, new-page write, new-page sync.
    auto fault_opened = DiskManager::open(failing, fault_path, false);
    assert(fault_opened.ok());
    auto fault_disk = std::move(fault_opened).value();
    assert(fault_disk->allocate_slotted_page().status().code() == ErrorCode::io_error);
    fault_disk.reset();
    reopened = DiskManager::open(*fs, fault_path, false);
    assert(reopened.ok());
    disk = std::move(reopened).value();
    assert(disk->page_count() == 1);
    assert(disk->allocate_slotted_page().value() == 1);
    std::array<std::vector<std::uint64_t>, 4> ids;
    std::vector<std::thread> workers;
    for (auto &worker_ids : ids) {
        workers.emplace_back([&disk, &worker_ids] {
            for (int index = 0; index < 10; ++index) {
                const auto allocated = disk->allocate_slotted_page();
                assert(allocated.ok());
                worker_ids.push_back(allocated.value());
            }
        });
    }
    for (auto &worker : workers)
        worker.join();
    assert(disk->page_count() == 42);
    std::vector<std::uint64_t> all_ids;
    for (const auto &worker_ids : ids) {
        all_ids.insert(all_ids.end(), worker_ids.begin(), worker_ids.end());
    }
    std::sort(all_ids.begin(), all_ids.end());
    assert(all_ids.size() == 40);
    for (std::size_t index = 0; index < all_ids.size(); ++index) {
        assert(all_ids[index] == index + 2);
    }
    for (std::uint64_t id = 1; id < disk->page_count(); ++id) {
        assert(disk->read_page(id).ok());
    }
    disk.reset();
    std::filesystem::remove(fault_path);
    std::filesystem::remove(path);
}
