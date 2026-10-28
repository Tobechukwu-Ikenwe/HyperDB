#pragma once

#include "hyperdb/status.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>

namespace hyperdb {

/** A file with positioned, complete-or-error I/O. Implementations are thread safe. */
class File {
  public:
    virtual ~File() = default;
    [[nodiscard]] virtual Status read_at(std::uint64_t offset,
                                         std::span<std::byte> bytes) noexcept = 0;
    [[nodiscard]] virtual Status write_at(std::uint64_t offset,
                                          std::span<const std::byte> bytes) noexcept = 0;
    [[nodiscard]] virtual Status sync() noexcept = 0;
    [[nodiscard]] virtual Result<std::uint64_t> size() noexcept = 0;
};

/** Opens files for the page-store path. create=true creates if absent, without truncation. */
class FileSystem {
  public:
    virtual ~FileSystem() = default;
    [[nodiscard]] virtual Result<std::unique_ptr<File>> open(const std::filesystem::path &path,
                                                             bool create) noexcept = 0;
};

/** Constructs the POSIX implementation. */
[[nodiscard]] std::unique_ptr<FileSystem> make_posix_file_system();

/** Fails one chosen operation (1-based) before forwarding to a wrapped file. */
[[nodiscard]] std::unique_ptr<File> make_fault_file(std::unique_ptr<File> delegate,
                                                    std::uint64_t fail_operation);

} // namespace hyperdb
