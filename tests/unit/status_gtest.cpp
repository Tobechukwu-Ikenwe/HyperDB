#include "hyperdb/status.h"

#include <gtest/gtest.h>

#include <memory>

namespace {

TEST(StatusTest, SuccessAndErrorCarryStableCodes) {
    const hyperdb::Status success;
    EXPECT_TRUE(success.ok());
    EXPECT_EQ(success.code(), hyperdb::ErrorCode::ok);

    const hyperdb::Status error{hyperdb::ErrorCode::io_error, "disk full"};
    EXPECT_FALSE(error.ok());
    EXPECT_EQ(error.code(), hyperdb::ErrorCode::io_error);
    EXPECT_EQ(error.message(), "disk full");
}

TEST(ResultTest, SupportsMoveOnlyValues) {
    hyperdb::Result<std::unique_ptr<int>> result{std::make_unique<int>(17)};
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(*result.value(), 17);
    const auto moved = std::move(result).value();
    ASSERT_NE(moved, nullptr);
    EXPECT_EQ(*moved, 17);
}

TEST(ResultTest, PreservesError) {
    const hyperdb::Result<int> result{
        hyperdb::Status{hyperdb::ErrorCode::not_found, "missing row"}};
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.status().code(), hyperdb::ErrorCode::not_found);
    EXPECT_EQ(result.status().message(), "missing row");
}

} // namespace
