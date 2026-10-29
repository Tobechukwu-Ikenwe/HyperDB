#include "storage/page_format.h"

#include "common/crc32c.h"

#include <algorithm>
#include <cstring>
#include <string_view>

namespace hyperdb {
namespace {

constexpr std::string_view kFileMagic = "HDBPAGE1";
constexpr std::uint32_t kFormatVersion = 1;
constexpr std::uint16_t kSlottedPageType = 1;
constexpr std::size_t kSlotSize = 4;
enum class FieldWidth : std::uint8_t { two = 2, four = 4, eight = 8 };

std::uint64_t load_le(std::span<const std::byte> bytes, std::size_t offset,
                      FieldWidth width) noexcept {
    std::uint64_t value = 0;
    for (unsigned index = 0; index < static_cast<unsigned>(width); ++index) {
        value |= static_cast<std::uint64_t>(bytes[offset + index]) << (8 * index);
    }
    return value;
}

void store_le(std::span<std::byte> bytes, std::size_t offset, std::uint64_t value,
              FieldWidth width) noexcept {
    for (unsigned index = 0; index < static_cast<unsigned>(width); ++index) {
        bytes[offset + index] = std::byte((value >> (8 * index)) & 0xffU);
    }
}

void seal(Page &page) noexcept {
    store_le(page, kPageChecksumOffset, crc32c(std::span(page).first(kPageChecksumOffset)),
             FieldWidth::four);
}

Status check_checksum(std::span<const std::byte> page) noexcept {
    if (page.size() != kPageSize) {
        return {ErrorCode::corruption, "Invalid page size"};
    }
    if (load_le(page, kPageChecksumOffset, FieldWidth::four) !=
        crc32c(page.first(kPageChecksumOffset))) {
        return {ErrorCode::corruption, "Page checksum mismatch"};
    }
    return {};
}

bool reserved_zero(std::span<const std::byte> bytes, std::size_t begin, std::size_t end) noexcept {
    const auto reserved = bytes.subspan(begin, end - begin);
    return std::all_of(reserved.begin(), reserved.end(),
                       [](std::byte value) { return value == std::byte{0}; });
}

} // namespace

Status init_file_header(Page &page, std::uint64_t page_count, std::uint64_t page_lsn) noexcept {
    if (page_count == 0) {
        return {ErrorCode::invalid_argument, "Page count must include header page"};
    }
    page.fill(std::byte{0});
    for (std::size_t index = 0; index < kFileMagic.size(); ++index) {
        page[index] = std::byte(static_cast<unsigned char>(kFileMagic[index]));
    }
    store_le(page, 8, kFormatVersion, FieldWidth::four);
    store_le(page, 12, kPageSize, FieldWidth::four);
    store_le(page, 16, page_count, FieldWidth::eight);
    store_le(page, 24, page_lsn, FieldWidth::eight);
    seal(page);
    return {};
}

Result<FileHeader> parse_file_header(std::span<const std::byte> page) noexcept {
    if (const auto checked = check_checksum(page); !checked.ok()) {
        return checked;
    }
    for (std::size_t index = 0; index < kFileMagic.size(); ++index) {
        if (page[index] != std::byte(static_cast<unsigned char>(kFileMagic[index]))) {
            return Status{ErrorCode::corruption, "Invalid page-file magic"};
        }
    }
    const FileHeader header{static_cast<std::uint32_t>(load_le(page, 8, FieldWidth::four)),
                            static_cast<std::uint32_t>(load_le(page, 12, FieldWidth::four)),
                            load_le(page, 16, FieldWidth::eight),
                            load_le(page, 24, FieldWidth::eight)};
    if (header.version != kFormatVersion || header.page_size != kPageSize ||
        header.page_count == 0 || !reserved_zero(page, 32, kPageChecksumOffset)) {
        return Status{ErrorCode::corruption, "Unsupported or invalid page-file header"};
    }
    return header;
}

Status set_page_count(Page &page, std::uint64_t page_count, std::uint64_t page_lsn) noexcept {
    const auto parsed = parse_file_header(page);
    if (!parsed.ok()) {
        return parsed.status();
    }
    if (page_count < parsed.value().page_count || page_lsn < parsed.value().page_lsn) {
        return {ErrorCode::invalid_argument, "Page count or pageLSN cannot decrease"};
    }
    store_le(page, 16, page_count, FieldWidth::eight);
    store_le(page, 24, page_lsn, FieldWidth::eight);
    seal(page);
    return {};
}

Status init_slotted_page(Page &page, std::uint64_t page_id, std::uint64_t page_lsn) noexcept {
    if (page_id == 0) {
        return {ErrorCode::invalid_argument, "Page zero is reserved for file metadata"};
    }
    page.fill(std::byte{0});
    store_le(page, 0, kSlottedPageType, FieldWidth::two);
    store_le(page, 4, page_id, FieldWidth::eight);
    store_le(page, 12, page_lsn, FieldWidth::eight);
    store_le(page, 20, kSlottedHeaderSize, FieldWidth::two);
    store_le(page, 22, kPageChecksumOffset, FieldWidth::two);
    seal(page);
    return {};
}

Result<SlottedPageHeader> parse_slotted_page(std::span<const std::byte> page) noexcept {
    if (const auto checked = check_checksum(page); !checked.ok()) {
        return checked;
    }
    if (load_le(page, 0, FieldWidth::two) != kSlottedPageType ||
        load_le(page, 2, FieldWidth::two) != 0 || !reserved_zero(page, 26, kSlottedHeaderSize)) {
        return Status{ErrorCode::corruption, "Unknown slotted-page type or flags"};
    }
    const SlottedPageHeader header{load_le(page, 4, FieldWidth::eight),
                                   load_le(page, 12, FieldWidth::eight),
                                   static_cast<std::uint16_t>(load_le(page, 20, FieldWidth::two)),
                                   static_cast<std::uint16_t>(load_le(page, 22, FieldWidth::two)),
                                   static_cast<std::uint16_t>(load_le(page, 24, FieldWidth::two))};
    if (header.page_id == 0 ||
        header.lower !=
            kSlottedHeaderSize + static_cast<std::size_t>(header.slot_count) * kSlotSize ||
        header.lower > header.upper || header.upper > kPageChecksumOffset) {
        return Status{ErrorCode::corruption, "Invalid slotted-page boundaries"};
    }
    for (std::size_t index = 0; index < header.slot_count; ++index) {
        const auto offset = load_le(page, kSlottedHeaderSize + index * kSlotSize, FieldWidth::two);
        const auto length =
            load_le(page, kSlottedHeaderSize + index * kSlotSize + 2, FieldWidth::two);
        if (length == 0 || offset < header.upper || offset + length > kPageChecksumOffset) {
            return Status{ErrorCode::corruption, "Invalid record span"};
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            const auto other_offset =
                load_le(page, kSlottedHeaderSize + prior * kSlotSize, FieldWidth::two);
            const auto other_length =
                load_le(page, kSlottedHeaderSize + prior * kSlotSize + 2, FieldWidth::two);
            if (offset < other_offset + other_length && other_offset < offset + length) {
                return Status{ErrorCode::corruption, "Overlapping record spans"};
            }
        }
    }
    return header;
}

Result<std::uint16_t> insert_record(Page &page, std::span<const std::byte> record,
                                    std::uint64_t page_lsn) noexcept {
    const auto parsed = parse_slotted_page(page);
    if (!parsed.ok()) {
        return parsed.status();
    }
    const auto header = parsed.value();
    if (record.empty() || page_lsn < header.page_lsn) {
        return Status{ErrorCode::invalid_argument, "Empty record or decreasing pageLSN"};
    }
    if (header.lower + kSlotSize > header.upper ||
        record.size() > header.upper - header.lower - kSlotSize) {
        return Status{ErrorCode::resource_exhausted, "Slotted page is full"};
    }
    const auto new_upper = header.upper - record.size();
    std::memmove(page.data() + new_upper, record.data(), record.size());
    store_le(page, header.lower, new_upper, FieldWidth::two);
    store_le(page, header.lower + 2, record.size(), FieldWidth::two);
    store_le(page, 20, header.lower + kSlotSize, FieldWidth::two);
    store_le(page, 22, new_upper, FieldWidth::two);
    store_le(page, 24, header.slot_count + 1, FieldWidth::two);
    store_le(page, 12, page_lsn, FieldWidth::eight);
    seal(page);
    return header.slot_count;
}

Result<std::span<const std::byte>> read_record(std::span<const std::byte> page,
                                               std::uint16_t slot) noexcept {
    const auto parsed = parse_slotted_page(page);
    if (!parsed.ok()) {
        return parsed.status();
    }
    if (slot >= parsed.value().slot_count) {
        return Status{ErrorCode::not_found, "Record slot not found"};
    }
    const auto offset = load_le(page, kSlottedHeaderSize + slot * kSlotSize, FieldWidth::two);
    const auto length = load_le(page, kSlottedHeaderSize + slot * kSlotSize + 2, FieldWidth::two);
    return page.subspan(offset, length);
}

} // namespace hyperdb
