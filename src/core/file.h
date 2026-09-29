#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace core {

std::optional<std::string> read_file(const std::filesystem::path &path);
bool write_file(const std::filesystem::path &path, std::string_view data, unsigned mode);
bool write_file_atomic(const std::filesystem::path &path, std::string_view data, unsigned mode);

} // namespace core
