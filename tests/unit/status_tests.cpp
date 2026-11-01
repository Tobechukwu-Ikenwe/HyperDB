#include "hyperdb/logging.h"
#include "hyperdb/status.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <string>

namespace {
int calls = 0;
void count_log(hyperdb::LogLevel level, std::string_view message) noexcept {
    assert(level == hyperdb::LogLevel::info);
    assert(message == "ready");
    ++calls;
}
} // namespace

int main() {
    using namespace hyperdb;
    Status success;
    assert(success.ok());
    assert(success.code() == ErrorCode::ok);
    Result<int> number{42};
    assert(number.ok() && number.value() == 42);
    Result<int> failure{Status{ErrorCode::io_error, "disk full"}};
    assert(!failure.ok());
    assert(failure.status().code() == ErrorCode::io_error);
    assert(failure.status().message() == "disk full");
    set_log_sink(count_log);
    log(LogLevel::info, "ready");
    set_log_sink(nullptr);
    log(LogLevel::info, "ready");
    assert(calls == 1);
}
