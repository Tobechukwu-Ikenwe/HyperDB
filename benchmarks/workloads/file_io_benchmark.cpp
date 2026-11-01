#include "hyperdb/file_system.h"

#include <benchmark/benchmark.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>

namespace {

void read_cached_page(benchmark::State &state) {
    const auto path = std::filesystem::temp_directory_path() /
                      ("hyperdb-io-benchmark-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    auto fs = hyperdb::make_posix_file_system();
    auto opened = fs->open(path, true);
    if (!opened.ok()) {
        state.SkipWithError(opened.status().message().c_str());
        return;
    }
    auto file = std::move(opened).value();
    std::array<std::byte, 8192> page{};
    const auto written = file->write_at(0, page);
    if (!written.ok()) {
        state.SkipWithError(written.message().c_str());
        std::filesystem::remove(path);
        return;
    }
    for (auto _ : state) {
        (void)_;
        const auto read = file->read_at(0, page);
        if (!read.ok()) {
            state.SkipWithError(read.message().c_str());
            break;
        }
        benchmark::DoNotOptimize(page.data());
    }
    file.reset();
    std::filesystem::remove(path);
}

BENCHMARK(read_cached_page);

} // namespace
