#include "hyperdb/logging.h"
#include "hyperdb/status.h"
#include <gtest/gtest.h>

#include <cassert>
#include <string>

namespace {
int calls = 0;
void count_log(hyperdb::LogLevel level, std::string_view message) noexcept {
    EXPECT_TRUE(level == hyperdb::LogLevel::info);
    EXPECT_TRUE(message == "ready");
    ++calls;
}
} // namespace

TEST(StatusTests, ExistingCases) {
    using namespace hyperdb;
    Status success;
    EXPECT_TRUE(success.ok());
    EXPECT_TRUE(success.code() == ErrorCode::ok);
    Result<int> number{42};
    EXPECT_TRUE(number.ok() && number.value() == 42);
    Result<int> failure{Status{ErrorCode::io_error, "disk full"}};
    EXPECT_TRUE(!failure.ok());
    EXPECT_TRUE(failure.status().code() == ErrorCode::io_error);
    EXPECT_TRUE(failure.status().message() == "disk full");
    set_log_sink(count_log);
    log(LogLevel::info, "ready");
    set_log_sink(nullptr);
    log(LogLevel::info, "ready");
    EXPECT_TRUE(calls == 1);
}
