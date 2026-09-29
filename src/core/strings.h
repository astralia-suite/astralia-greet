#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace core {

std::string_view trim(std::string_view text);
std::vector<std::string> split(std::string_view text, std::string_view separators);
std::vector<std::string> split_words(std::string_view text);
std::string join(const std::vector<std::string> &parts, std::string_view separator);
std::string lowercase(std::string_view text);
std::optional<bool> parse_bool(std::string_view text);
std::optional<int> parse_int(std::string_view text);
std::optional<double> parse_double(std::string_view text);
std::size_t utf8_length(std::string_view text);
std::string utf8_first(std::string_view text);
void utf8_pop_back(std::string &text);
void secure_clear(std::string &text);

} // namespace core
