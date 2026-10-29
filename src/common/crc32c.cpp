#include "common/crc32c.h"

namespace hyperdb {

std::uint32_t crc32c(std::span<const std::byte> bytes) noexcept {
    std::uint32_t crc = 0xffffffffU;
    for (const auto byte : bytes) {
        crc ^= static_cast<std::uint32_t>(byte);
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1U) ? 0x82f63b78U : 0U);
        }
    }
    return ~crc;
}

} // namespace hyperdb
