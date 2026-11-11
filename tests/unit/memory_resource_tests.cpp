#include "hyperdb/memory.h"
#include <gtest/gtest.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>

namespace {
struct CallbackLedger {
    std::size_t allocations = 0;
    std::size_t deallocations = 0;
};

std::size_t normalized_alignment(std::size_t alignment) {
    return alignment < alignof(std::max_align_t) ? alignof(std::max_align_t) : alignment;
}

void *callback_allocate(void *context, std::size_t bytes, std::size_t alignment) noexcept {
    auto &ledger = *static_cast<CallbackLedger *>(context);
    void *pointer = ::operator new(bytes == 0 ? 1 : bytes,
                                   std::align_val_t{normalized_alignment(alignment)}, std::nothrow);
    if (pointer)
        ++ledger.allocations;
    return pointer;
}

void callback_deallocate(void *context, void *pointer, std::size_t,
                         std::size_t alignment) noexcept {
    if (!pointer)
        return;
    auto &ledger = *static_cast<CallbackLedger *>(context);
    ++ledger.deallocations;
    ::operator delete(pointer, std::align_val_t{normalized_alignment(alignment)});
}

alignas(64) unsigned char misaligned_storage[128];

void *misaligned_allocate(void *, std::size_t, std::size_t) noexcept {
    return misaligned_storage + 1;
}

void misaligned_deallocate(void *context, void *, std::size_t, std::size_t) noexcept {
    ++static_cast<CallbackLedger *>(context)->deallocations;
}
} // namespace

TEST(MemoryResourceTests, ExistingCases) {
    {
        hyperdb::MemoryResource resource;
        void *pointer = resource.allocate(48, 128);
        EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(pointer) % 128 == 0);
        auto stats = resource.stats();
        EXPECT_TRUE(stats.current_bytes == 48);
        EXPECT_TRUE(stats.peak_bytes == 48);
        EXPECT_TRUE(stats.allocation_count == 1);
        EXPECT_TRUE(!stats.remaining_budget);
        resource.deallocate(pointer, 48, 128);
        stats = resource.stats();
        EXPECT_TRUE(stats.current_bytes == 0);
        EXPECT_TRUE(stats.deallocation_count == 1);
    }

    {
        CallbackLedger ledger;
        hyperdb::AllocatorConfig config;
        config.backend = hyperdb::AllocatorBackend::application;
        config.budget_bytes = 96;
        config.context = &ledger;
        config.allocate = callback_allocate;
        config.deallocate = callback_deallocate;
        hyperdb::MemoryResource resource(config);

        void *first = resource.allocate(64, 64);
        void *second = resource.allocate(32, alignof(std::max_align_t));
        EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(first) % 64 == 0);
        EXPECT_TRUE(resource.stats().remaining_budget == 0);
        bool exhausted = false;
        try {
            (void)resource.allocate(1, alignof(std::max_align_t));
        } catch (const std::bad_alloc &) {
            exhausted = true;
        }
        EXPECT_TRUE(exhausted);
        auto stats = resource.stats();
        EXPECT_TRUE(stats.current_bytes == 96);
        EXPECT_TRUE(stats.peak_bytes == 96);
        EXPECT_TRUE(stats.allocation_count == 2);
        EXPECT_TRUE(stats.failure_count == 1);
        resource.deallocate(second, 32, alignof(std::max_align_t));
        resource.deallocate(first, 64, 64);
        stats = resource.stats();
        EXPECT_TRUE(stats.current_bytes == 0);
        EXPECT_TRUE(stats.deallocation_count == 2);
        EXPECT_TRUE(ledger.allocations == 2);
        EXPECT_TRUE(ledger.deallocations == 2);
    }

    {
        CallbackLedger ledger;
        hyperdb::AllocatorConfig config;
        config.backend = hyperdb::AllocatorBackend::application;
        config.context = &ledger;
        config.allocate = callback_allocate;
        config.deallocate = callback_deallocate;
        hyperdb::MemoryResource left(config);
        hyperdb::MemoryResource right(config);
        void *pointer = left.allocate(16, alignof(std::max_align_t));
        EXPECT_TRUE(left.stats().current_bytes == 16);
        EXPECT_TRUE(right.stats().current_bytes == 0);
        left.deallocate(pointer, 16, alignof(std::max_align_t));
    }

    {
        CallbackLedger ledger;
        hyperdb::AllocatorConfig config;
        config.backend = hyperdb::AllocatorBackend::application;
        config.context = &ledger;
        config.allocate = misaligned_allocate;
        config.deallocate = misaligned_deallocate;
        hyperdb::MemoryResource resource(config);
        bool rejected = false;
        try {
            (void)resource.allocate(32, 64);
        } catch (const std::bad_alloc &) {
            rejected = true;
        }
        EXPECT_TRUE(rejected);
        EXPECT_TRUE(resource.stats().current_bytes == 0);
        EXPECT_TRUE(resource.stats().failure_count == 1);
        EXPECT_TRUE(ledger.deallocations == 1);
    }

    if (hyperdb::MemoryResource::hypermalloc_available()) {
        hyperdb::AllocatorConfig config;
        config.backend = hyperdb::AllocatorBackend::hypermalloc;
        config.budget_bytes = 128;
        hyperdb::MemoryResource resource(config);
        void *pointer = resource.allocate(96, 64);
        EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(pointer) % 64 == 0);
        EXPECT_TRUE(resource.stats().current_bytes == 96);
        resource.deallocate(pointer, 96, 64);
        EXPECT_TRUE(resource.stats().current_bytes == 0);
    }
}
