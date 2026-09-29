#include "core/file.h"

#include <cstdio>
#include <fcntl.h>
#include <fstream>
#include <iterator>
#include <sys/stat.h>
#include <unistd.h>

namespace core {

std::optional<std::string> read_file(const std::filesystem::path &path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

bool write_file(const std::filesystem::path &path, std::string_view data, unsigned mode) {
    int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode);
    if (fd < 0) {
        return false;
    }
    fchmod(fd, mode);
    std::size_t written = 0;
    while (written < data.size()) {
        auto n = ::write(fd, data.data() + written, data.size() - written);
        if (n <= 0) {
            ::close(fd);
            return false;
        }
        written += static_cast<std::size_t>(n);
    }
    return ::close(fd) == 0;
}

bool write_file_atomic(const std::filesystem::path &path, std::string_view data, unsigned mode) {
    auto temporary = path;
    temporary += ".tmp";
    if (!write_file(temporary, data, mode)) {
        return false;
    }
    return std::rename(temporary.c_str(), path.c_str()) == 0;
}

} // namespace core
