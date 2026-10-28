#pragma once

#include <cassert>
#include <optional>
#include <string>
#include <utility>

namespace hyperdb {

enum class ErrorCode {
    ok,
    invalid_argument,
    not_found,
    io_error,
    corruption,
    resource_exhausted,
    internal
};

/** A value type for errors returned across the public API boundary. */
class Status {
  public:
    Status() = default;
    Status(ErrorCode code, std::string message) : code_(code), message_(std::move(message)) {
        assert(code != ErrorCode::ok || message_.empty());
    }

    [[nodiscard]] bool ok() const noexcept { return code_ == ErrorCode::ok; }
    [[nodiscard]] ErrorCode code() const noexcept { return code_; }
    [[nodiscard]] const std::string &message() const noexcept { return message_; }

  private:
    ErrorCode code_ = ErrorCode::ok;
    std::string message_;
};

/** Holds either a value or an error. Callers check ok() before value(). */
template <typename T> class Result {
  public:
    Result(T value) : value_(std::move(value)) {}
    Result(Status status) : status_(std::move(status)) { assert(!status_.ok()); }

    [[nodiscard]] bool ok() const noexcept { return value_.has_value(); }
    [[nodiscard]] const Status &status() const noexcept { return status_; }
    [[nodiscard]] T &value() & {
        assert(ok());
        return *value_;
    }
    [[nodiscard]] const T &value() const & {
        assert(ok());
        return *value_;
    }
    [[nodiscard]] T &&value() && {
        assert(ok());
        return std::move(*value_);
    }

  private:
    std::optional<T> value_;
    Status status_;
};

} // namespace hyperdb
