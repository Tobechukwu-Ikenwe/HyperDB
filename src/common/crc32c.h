#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace hyperdb {

/** Portable Castagnoli CRC32C, initialized and finalized with all bits set. */
[[nodiscard]] std::uint32_t crc32c(std::span<const std::byte> bytes) noexcept;

} // namespace hyperdb
