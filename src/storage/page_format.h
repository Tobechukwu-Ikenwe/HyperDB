#pragma once

#include "hyperdb/status.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace hyperdb {

inline constexpr std::size_t kPageSize = 8192;
inline constexpr std::size_t kPageChecksumOffset = kPageSize - sizeof(std::uint32_t);
inline constexpr std::size_t kSlottedHeaderSize = 32;
using Page = std::array<std::byte, kPageSize>;

struct FileHeader {
    std::uint32_t version;
    std::uint32_t page_size;
    std::uint64_t page_count;
    std::uint64_t page_lsn;
};

struct SlottedPageHeader {
    std::uint64_t page_id;
    std::uint64_t page_lsn;
    std::uint16_t lower;
    std::uint16_t upper;
    std::uint16_t slot_count;
};

/** Initializes page zero with a version-1 file header and a checksum. */
[[nodiscard]] Status init_file_header(Page &page, std::uint64_t page_count = 1,
                                      std::uint64_t page_lsn = 0) noexcept;

/** Validates and decodes page zero. */
[[nodiscard]] Result<FileHeader> parse_file_header(std::span<const std::byte> page) noexcept;

/** Updates page zero after allocation; rejects decreasing count or pageLSN. */
[[nodiscard]] Status set_page_count(Page &page, std::uint64_t page_count,
                                    std::uint64_t page_lsn) noexcept;

/** Initializes an empty slotted page with a checksum. Page IDs start at one. */
[[nodiscard]] Status init_slotted_page(Page &page, std::uint64_t page_id,
                                       std::uint64_t page_lsn = 0) noexcept;

/** Validates checksum, boundaries, slots, and live record spans. */
[[nodiscard]] Result<SlottedPageHeader>
parse_slotted_page(std::span<const std::byte> page) noexcept;

/** Appends a record and returns its zero-based slot ID. */
[[nodiscard]] Result<std::uint16_t> insert_record(Page &page, std::span<const std::byte> record,
                                                  std::uint64_t page_lsn) noexcept;

/** Returns a borrowed record span; the input page must outlive the result. */
[[nodiscard]] Result<std::span<const std::byte>> read_record(std::span<const std::byte> page,
                                                             std::uint16_t slot) noexcept;

} // namespace hyperdb
