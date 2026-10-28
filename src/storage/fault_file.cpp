#include "hyperdb/file_system.h"

#include <atomic>

namespace hyperdb {
namespace {
class FaultFile final : public File {
  public:
    FaultFile(std::unique_ptr<File> delegate, std::uint64_t fail_operation)
        : delegate_(std::move(delegate)), fail_operation_(fail_operation) {}

    Status read_at(std::uint64_t offset, std::span<std::byte> bytes) noexcept override {
        if (fail())
            return injected();
        return delegate_->read_at(offset, bytes);
    }
    Status write_at(std::uint64_t offset, std::span<const std::byte> bytes) noexcept override {
        if (fail())
            return injected();
        return delegate_->write_at(offset, bytes);
    }
    Status sync() noexcept override {
        if (fail())
            return injected();
        return delegate_->sync();
    }
    Result<std::uint64_t> size() noexcept override {
        if (fail())
            return injected();
        return delegate_->size();
    }

  private:
    bool fail() noexcept {
        return calls_.fetch_add(1, std::memory_order_relaxed) + 1 == fail_operation_;
    }
    static Status injected() { return {ErrorCode::io_error, "Injected file I/O failure"}; }
    std::unique_ptr<File> delegate_;
    std::uint64_t fail_operation_;
    std::atomic<std::uint64_t> calls_{0};
};
} // namespace

std::unique_ptr<File> make_fault_file(std::unique_ptr<File> delegate,
                                      std::uint64_t fail_operation) {
    return std::make_unique<FaultFile>(std::move(delegate), fail_operation);
}
} // namespace hyperdb
