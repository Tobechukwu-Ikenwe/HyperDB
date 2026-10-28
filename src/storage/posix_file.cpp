#include "hyperdb/file_system.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <fcntl.h>
#include <limits>
#include <new>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace hyperdb {
namespace {
Status io_error(const char *operation) {
    const int error = errno;
    return {ErrorCode::io_error,
            std::string(operation) + " failed (errno " + std::to_string(error) + ")"};
}

bool valid_range(std::uint64_t offset, std::size_t count) {
    constexpr auto maximum = static_cast<std::uint64_t>(std::numeric_limits<off_t>::max());
    return offset <= maximum && count <= maximum - offset;
}

class PosixFile final : public File {
  public:
    explicit PosixFile(int fd) : fd_(fd) {}
    ~PosixFile() override { (void)::close(fd_); }

    Status read_at(std::uint64_t offset, std::span<std::byte> bytes) noexcept override {
        if (!valid_range(offset, bytes.size()))
            return {ErrorCode::invalid_argument, "Read offset overflow"};
        std::size_t done = 0;
        while (done < bytes.size()) {
            const auto amount =
                static_cast<std::size_t>(std::min<std::uint64_t>(bytes.size() - done, SSIZE_MAX));
            const auto n =
                ::pread(fd_, bytes.data() + done, amount, static_cast<off_t>(offset + done));
            if (n < 0 && errno == EINTR)
                continue;
            if (n < 0)
                return io_error("pread");
            if (n == 0)
                return {ErrorCode::corruption, "Read past end of file"};
            done += static_cast<std::size_t>(n);
        }
        return {};
    }

    Status write_at(std::uint64_t offset, std::span<const std::byte> bytes) noexcept override {
        if (!valid_range(offset, bytes.size()))
            return {ErrorCode::invalid_argument, "Write offset overflow"};
        std::size_t done = 0;
        while (done < bytes.size()) {
            const auto amount =
                static_cast<std::size_t>(std::min<std::uint64_t>(bytes.size() - done, SSIZE_MAX));
            const auto n =
                ::pwrite(fd_, bytes.data() + done, amount, static_cast<off_t>(offset + done));
            if (n < 0 && errno == EINTR)
                continue;
            if (n < 0)
                return io_error("pwrite");
            if (n == 0)
                return {ErrorCode::io_error, "pwrite made no progress"};
            done += static_cast<std::size_t>(n);
        }
        return {};
    }

    Status sync() noexcept override {
        while (::fsync(fd_) != 0) {
            if (errno == EINTR)
                continue;
            return io_error("fsync");
        }
        return {};
    }

    Result<std::uint64_t> size() noexcept override {
        struct stat info{};
        if (::fstat(fd_, &info) != 0)
            return io_error("fstat");
        if (info.st_size < 0)
            return Status{ErrorCode::corruption, "Negative file size"};
        return static_cast<std::uint64_t>(info.st_size);
    }

  private:
    int fd_;
};

class PosixFileSystem final : public FileSystem {
  public:
    Result<std::unique_ptr<File>> open(const std::filesystem::path &path,
                                       bool create) noexcept override {
        if (path.empty())
            return Status{ErrorCode::invalid_argument, "Empty file path"};
        const int flags = O_RDWR | (create ? O_CREAT : 0);
        int fd;
        do {
            fd = ::open(path.c_str(), flags, 0600);
        } while (fd < 0 && errno == EINTR);
        if (fd < 0)
            return io_error("open");
        auto file = std::unique_ptr<File>(new (std::nothrow) PosixFile(fd));
        if (!file) {
            (void)::close(fd);
            return Status{ErrorCode::resource_exhausted, "File handle allocation failed"};
        }
        return file;
    }
};
} // namespace

std::unique_ptr<FileSystem> make_posix_file_system() { return std::make_unique<PosixFileSystem>(); }
} // namespace hyperdb
