#pragma once

#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <string_view>
#include <unistd.h>

namespace test {

inline int failures = 0;
inline int checks = 0;

inline std::filesystem::path temp_dir(std::string_view name) {
    auto dir = std::filesystem::temp_directory_path() / std::format("astralia-greet-test-{}-{}", getpid(), name);
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

inline void write_text(const std::filesystem::path &path, std::string_view text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path) << text;
}

} // namespace test

#define CHECK(expr)                                                                       \
    do {                                                                                  \
        ++test::checks;                                                                   \
        if (!(expr)) {                                                                    \
            ++test::failures;                                                             \
            std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #expr); \
        }                                                                                 \
    } while (false)
