#pragma once

#ifdef HYPERDB_SINGLE_THREADED
namespace hyperdb {

/** No-op BasicLockable for applications that guarantee single-threaded access. */
class Mutex final {
  public:
    void lock() noexcept {}
    void unlock() noexcept {}
};

} // namespace hyperdb
#else
#include <mutex>

namespace hyperdb {
using Mutex = std::mutex;
} // namespace hyperdb
#endif
