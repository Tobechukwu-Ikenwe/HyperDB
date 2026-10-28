#include "hyperdb/logging.h"

#include <atomic>

namespace hyperdb {
namespace {
std::atomic<LogSink> current_sink{nullptr};
}

void set_log_sink(LogSink sink) noexcept { current_sink.store(sink, std::memory_order_release); }

void log(LogLevel level, std::string_view message) noexcept {
    if (const auto sink = current_sink.load(std::memory_order_acquire))
        sink(level, message);
}
} // namespace hyperdb
