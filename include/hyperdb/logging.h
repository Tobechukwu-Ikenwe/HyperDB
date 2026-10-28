#pragma once

#include <string_view>

namespace hyperdb {

enum class LogLevel { error, warning, info, debug };
using LogSink = void (*)(LogLevel level, std::string_view message) noexcept;

/** Sets a process-wide callback. A null sink disables logging. */
void set_log_sink(LogSink sink) noexcept;

/** Sends a message to the configured sink, if any. */
void log(LogLevel level, std::string_view message) noexcept;

} // namespace hyperdb
