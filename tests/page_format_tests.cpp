#include "common/crc32c.h"
#include "storage/page_format.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace {

void refresh_checksum(hyperdb::Page &page) {
    const auto checksum =
        hyperdb::crc32c(std::span<const std::byte>(page).first(hyperdb::kPageChecksumOffset));
    for (unsigned i = 0; i < 4; ++i) {
        page[hyperdb::kPageChecksumOffset + i] = std::byte((checksum >> (8 * i)) & 0xff);
    }
}

void test_crc32c() {
    constexpr std::string_view input = "123456789";
    const auto bytes = std::as_bytes(std::span(input.data(), input.size()));
    assert(hyperdb::crc32c(bytes) == 0xe3069283U);
}

void test_file_header() {
    hyperdb::Page page{};
    assert(hyperdb::init_file_header(page, 1, 7).ok());
    assert(page[0] == std::byte{'H'} && page[7] == std::byte{'1'});
    assert(page[8] == std::byte{1} && page[12] == std::byte{0});
    assert(page[16] == std::byte{1} && page[24] == std::byte{7});
    const auto header = hyperdb::parse_file_header(page);
    assert(header.ok() && header.value().page_count == 1 && header.value().page_lsn == 7);
    assert(hyperdb::set_page_count(page, 4, 8).ok());
    assert(hyperdb::parse_file_header(page).value().page_count == 4);
    assert(!hyperdb::set_page_count(page, 3, 9).ok());
    assert(!hyperdb::set_page_count(page, 5, 7).ok());
    page[9] ^= std::byte{1};
    assert(hyperdb::parse_file_header(page).status().code() == hyperdb::ErrorCode::corruption);
    refresh_checksum(page);
    assert(hyperdb::parse_file_header(page).status().code() == hyperdb::ErrorCode::corruption);
    assert(hyperdb::parse_file_header(std::span(page).first(100)).status().code() ==
           hyperdb::ErrorCode::corruption);
}

void test_slotted_page() {
    hyperdb::Page page{};
    assert(!hyperdb::init_slotted_page(page, 0).ok());
    assert(hyperdb::init_slotted_page(page, 9, 11).ok());
    const auto empty = hyperdb::parse_slotted_page(page);
    assert(empty.ok() && empty.value().page_id == 9 && empty.value().slot_count == 0);
    assert(empty.value().lower == hyperdb::kSlottedHeaderSize);
    assert(empty.value().upper == hyperdb::kPageChecksumOffset);

    const std::array<std::byte, 3> first{std::byte{1}, std::byte{2}, std::byte{3}};
    const std::array<std::byte, 2> second{std::byte{4}, std::byte{5}};
    assert(hyperdb::insert_record(page, first, 12).value() == 0);
    assert(hyperdb::insert_record(page, second, 13).value() == 1);
    assert(hyperdb::read_record(page, 0).value().size() == 3);
    assert(hyperdb::read_record(page, 1).value()[0] == std::byte{4});
    assert(hyperdb::parse_slotted_page(page).value().page_lsn == 13);
    assert(hyperdb::read_record(page, 2).status().code() == hyperdb::ErrorCode::not_found);
    assert(hyperdb::insert_record(page, first, 12).status().code() ==
           hyperdb::ErrorCode::invalid_argument);

    auto corrupt = page;
    corrupt[hyperdb::kPageChecksumOffset - 1] ^= std::byte{1};
    assert(hyperdb::parse_slotted_page(corrupt).status().code() == hyperdb::ErrorCode::corruption);

    corrupt = page;
    corrupt[22] = std::byte{0};
    corrupt[23] = std::byte{0};
    refresh_checksum(corrupt);
    assert(hyperdb::parse_slotted_page(corrupt).status().code() == hyperdb::ErrorCode::corruption);

    corrupt = page;
    corrupt[36] = corrupt[32];
    corrupt[37] = corrupt[33];
    refresh_checksum(corrupt);
    assert(hyperdb::parse_slotted_page(corrupt).status().code() == hyperdb::ErrorCode::corruption);
}

void test_page_capacity() {
    hyperdb::Page page{};
    assert(hyperdb::init_slotted_page(page, 1).ok());
    std::vector<std::byte> exact_fit(hyperdb::kPageChecksumOffset - hyperdb::kSlottedHeaderSize -
                                     4);
    assert(hyperdb::insert_record(page, exact_fit, 1).ok());
    assert(hyperdb::read_record(page, 0).value().size() == exact_fit.size());
    assert(hyperdb::insert_record(page, std::span(exact_fit).first(1), 2).status().code() ==
           hyperdb::ErrorCode::resource_exhausted);
    assert(hyperdb::parse_slotted_page(page).value().slot_count == 1);
}

} // namespace

int main() {
    test_crc32c();
    test_file_header();
    test_slotted_page();
    test_page_capacity();
}
