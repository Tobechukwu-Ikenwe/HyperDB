#ifndef HYPERDB_MEMORY_H
#define HYPERDB_MEMORY_H

#include <atomic>
#include <cstddef>
#include <memory_resource>
#include <optional>

namespace hyperdb {

enum class AllocatorBackend { system, application, hypermalloc };

using AllocateCallback = void *(*)(void *context, std::size_t bytes,
                                   std::size_t alignment) noexcept;
using DeallocateCallback = void (*)(void *context, void *pointer, std::size_t bytes,
                                    std::size_t alignment) noexcept;

struct AllocatorConfig {
    AllocatorBackend backend = AllocatorBackend::system;
    std::size_t budget_bytes = 0;
    void *context = nullptr;
    AllocateCallback allocate = nullptr;
    DeallocateCallback deallocate = nullptr;
};

struct AllocatorStats {
    std::size_t current_bytes = 0;
    std::size_t peak_bytes = 0;
    std::size_t allocation_count = 0;
    std::size_t deallocation_count = 0;
    std::size_t failure_count = 0;
    std::optional<std::size_t> remaining_budget;
};

// Owns the accounting and adapts the selected backend to the standard PMR
// interface. Allocations must be deallocated through this same resource.
class MemoryResource final : public std::pmr::memory_resource {
  public:
    explicit MemoryResource(AllocatorConfig config = {});
    ~MemoryResource() override = default;

    MemoryResource(const MemoryResource &) = delete;
    MemoryResource &operator=(const MemoryResource &) = delete;

    [[nodiscard]] AllocatorStats stats() const noexcept;
    [[nodiscard]] static bool hypermalloc_available() noexcept;

  private:
    void *do_allocate(std::size_t bytes, std::size_t alignment) override;
    void do_deallocate(void *pointer, std::size_t bytes, std::size_t alignment) override;
    [[nodiscard]] bool do_is_equal(const std::pmr::memory_resource &other) const noexcept override;

    AllocatorConfig config_;
    std::atomic_size_t current_bytes_{0};
    std::atomic_size_t peak_bytes_{0};
    std::atomic_size_t allocation_count_{0};
    std::atomic_size_t deallocation_count_{0};
    std::atomic_size_t failure_count_{0};
};

} // namespace hyperdb

#endif
