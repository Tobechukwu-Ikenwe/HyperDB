#include "hyperdb/memory.h"

#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>

#if defined(HYPERDB_HAS_HYPERMALLOC)
#include <hypermalloc.h>
#endif

namespace hyperdb {

MemoryResource::MemoryResource(AllocatorConfig config) : config_(config) {
    switch (config_.backend) {
    case AllocatorBackend::system:
        break;
    case AllocatorBackend::application:
        if (!config_.allocate || !config_.deallocate)
            throw std::invalid_argument(
                "Application allocator requires allocate and deallocate callbacks");
        break;
    case AllocatorBackend::hypermalloc:
        if (!hypermalloc_available())
            throw std::invalid_argument("HyperMalloc backend is not enabled in this build");
        break;
    default:
        throw std::invalid_argument("Unknown allocator backend");
    }
}

bool MemoryResource::hypermalloc_available() noexcept {
#if defined(HYPERDB_HAS_HYPERMALLOC)
    return true;
#else
    return false;
#endif
}

void *MemoryResource::do_allocate(std::size_t bytes, std::size_t alignment) {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        failure_count_.fetch_add(1, std::memory_order_relaxed);
        throw std::bad_alloc{};
    }
    auto current = current_bytes_.load(std::memory_order_relaxed);
    for (;;) {
        if (bytes > std::numeric_limits<std::size_t>::max() - current ||
            (config_.budget_bytes != 0 && bytes > config_.budget_bytes - current)) {
            failure_count_.fetch_add(1, std::memory_order_relaxed);
            throw std::bad_alloc{};
        }
        if (current_bytes_.compare_exchange_weak(
                current, current + bytes, std::memory_order_acq_rel, std::memory_order_relaxed))
            break;
    }

    void *pointer = nullptr;
    try {
        switch (config_.backend) {
        case AllocatorBackend::system:
            pointer = std::pmr::new_delete_resource()->allocate(bytes, alignment);
            break;
        case AllocatorBackend::application:
            pointer = config_.allocate(config_.context, bytes, alignment);
            break;
        case AllocatorBackend::hypermalloc:
#if defined(HYPERDB_HAS_HYPERMALLOC)
            pointer = alignment <= 16 ? hm_malloc(bytes) : hm_memalign(alignment, bytes);
#endif
            break;
        }
    } catch (...) {
        current_bytes_.fetch_sub(bytes, std::memory_order_acq_rel);
        failure_count_.fetch_add(1, std::memory_order_relaxed);
        throw;
    }

    if (!pointer) {
        current_bytes_.fetch_sub(bytes, std::memory_order_acq_rel);
        failure_count_.fetch_add(1, std::memory_order_relaxed);
        throw std::bad_alloc{};
    }
    if (reinterpret_cast<std::uintptr_t>(pointer) % alignment != 0) {
        switch (config_.backend) {
        case AllocatorBackend::system:
            std::pmr::new_delete_resource()->deallocate(pointer, bytes, alignment);
            break;
        case AllocatorBackend::application:
            config_.deallocate(config_.context, pointer, bytes, alignment);
            break;
        case AllocatorBackend::hypermalloc:
#if defined(HYPERDB_HAS_HYPERMALLOC)
            hm_free(pointer);
#endif
            break;
        }
        current_bytes_.fetch_sub(bytes, std::memory_order_acq_rel);
        failure_count_.fetch_add(1, std::memory_order_relaxed);
        throw std::bad_alloc{};
    }

    allocation_count_.fetch_add(1, std::memory_order_relaxed);
    auto peak = peak_bytes_.load(std::memory_order_relaxed);
    while (peak < current + bytes &&
           !peak_bytes_.compare_exchange_weak(peak, current + bytes, std::memory_order_relaxed,
                                              std::memory_order_relaxed)) {
    }
    return pointer;
}

void MemoryResource::do_deallocate(void *pointer, std::size_t bytes, std::size_t alignment) {
    if (!pointer)
        return;
    switch (config_.backend) {
    case AllocatorBackend::system:
        std::pmr::new_delete_resource()->deallocate(pointer, bytes, alignment);
        break;
    case AllocatorBackend::application:
        config_.deallocate(config_.context, pointer, bytes, alignment);
        break;
    case AllocatorBackend::hypermalloc:
#if defined(HYPERDB_HAS_HYPERMALLOC)
        hm_free(pointer);
#endif
        break;
    }
    current_bytes_.fetch_sub(bytes, std::memory_order_acq_rel);
    deallocation_count_.fetch_add(1, std::memory_order_relaxed);
}

AllocatorStats MemoryResource::stats() const noexcept {
    AllocatorStats result;
    result.current_bytes = current_bytes_.load(std::memory_order_relaxed);
    result.peak_bytes = peak_bytes_.load(std::memory_order_relaxed);
    result.allocation_count = allocation_count_.load(std::memory_order_relaxed);
    result.deallocation_count = deallocation_count_.load(std::memory_order_relaxed);
    result.failure_count = failure_count_.load(std::memory_order_relaxed);
    if (config_.budget_bytes != 0)
        result.remaining_budget = config_.budget_bytes - result.current_bytes;
    return result;
}

bool MemoryResource::do_is_equal(const std::pmr::memory_resource &other) const noexcept {
    return this == &other;
}

} // namespace hyperdb
