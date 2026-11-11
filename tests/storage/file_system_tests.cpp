#include "hyperdb/file_system.h"
#include <gtest/gtest.h>

#include <array>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <limits>

TEST(FileSystemTests, ExistingCases) {
    using namespace hyperdb;
    const auto path = std::filesystem::temp_directory_path() /
                      ("hyperdb-file-test-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    auto fs = make_posix_file_system();
    EXPECT_TRUE(!fs->open(path, false).ok());
    auto opened = fs->open(path, true);
    EXPECT_TRUE(opened.ok());
    auto file = std::move(opened).value();
    std::array<std::byte, 4> data{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    EXPECT_TRUE(file->write_at(0, data).ok());
    EXPECT_TRUE(file->sync().ok());
    EXPECT_TRUE(file->size().value() == data.size());
    std::array<std::byte, 5> too_long{};
    EXPECT_TRUE(file->read_at(0, too_long).code() == ErrorCode::corruption);
    EXPECT_TRUE(file->write_at(std::numeric_limits<std::uint64_t>::max(), data).code() ==
                ErrorCode::invalid_argument);
    auto fault = make_fault_file(std::move(file), 2);
    EXPECT_TRUE(fault->size().ok());
    EXPECT_TRUE(fault->sync().code() == ErrorCode::io_error);
    EXPECT_TRUE(fault->sync().ok());
    fault.reset();
    auto reopened = fs->open(path, false);
    EXPECT_TRUE(reopened.ok());
    std::array<std::byte, 4> actual{};
    EXPECT_TRUE(std::move(reopened).value()->read_at(0, actual).ok());
    EXPECT_TRUE(actual == data);
    auto write_target = fs->open(path, false);
    EXPECT_TRUE(write_target.ok());
    auto write_fault = make_fault_file(std::move(write_target).value(), 1);
    EXPECT_TRUE(write_fault->write_at(0, data).code() == ErrorCode::io_error);
    EXPECT_TRUE(write_fault->read_at(0, actual).ok());
    auto read_target = fs->open(path, false);
    EXPECT_TRUE(read_target.ok());
    auto read_fault = make_fault_file(std::move(read_target).value(), 1);
    EXPECT_TRUE(read_fault->read_at(0, actual).code() == ErrorCode::io_error);
    std::filesystem::remove(path);
}
