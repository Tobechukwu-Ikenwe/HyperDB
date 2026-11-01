#include "hyperdb/file_system.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <limits>

int main() {
    using namespace hyperdb;
    const auto path = std::filesystem::temp_directory_path() /
                      ("hyperdb-file-test-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    auto fs = make_posix_file_system();
    assert(!fs->open(path, false).ok());
    auto opened = fs->open(path, true);
    assert(opened.ok());
    auto file = std::move(opened).value();
    std::array<std::byte, 4> data{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    assert(file->write_at(0, data).ok());
    assert(file->sync().ok());
    assert(file->size().value() == data.size());
    std::array<std::byte, 5> too_long{};
    assert(file->read_at(0, too_long).code() == ErrorCode::corruption);
    assert(file->write_at(std::numeric_limits<std::uint64_t>::max(), data).code() ==
           ErrorCode::invalid_argument);
    auto fault = make_fault_file(std::move(file), 2);
    assert(fault->size().ok());
    assert(fault->sync().code() == ErrorCode::io_error);
    assert(fault->sync().ok());
    fault.reset();
    auto reopened = fs->open(path, false);
    assert(reopened.ok());
    std::array<std::byte, 4> actual{};
    assert(std::move(reopened).value()->read_at(0, actual).ok());
    assert(actual == data);
    auto write_target = fs->open(path, false);
    assert(write_target.ok());
    auto write_fault = make_fault_file(std::move(write_target).value(), 1);
    assert(write_fault->write_at(0, data).code() == ErrorCode::io_error);
    assert(write_fault->read_at(0, actual).ok());
    auto read_target = fs->open(path, false);
    assert(read_target.ok());
    auto read_fault = make_fault_file(std::move(read_target).value(), 1);
    assert(read_fault->read_at(0, actual).code() == ErrorCode::io_error);
    std::filesystem::remove(path);
}
