#include "core/strings.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstring>

namespace core {

std::string_view trim(std::string_view text) {
    constexpr std::string_view spaces = " \t\r\n\f\v";
    auto begin = text.find_first_not_of(spaces);
    if (begin == std::string_view::npos) {
        return {};
    }
    auto end = text.find_last_not_of(spaces);
    return text.substr(begin, end - begin + 1);
}

std::vector<std::string> split(std::string_view text, std::string_view separators) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= text.size()) {
        auto end = text.find_first_of(separators, start);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        auto part = trim(text.substr(start, end - start));
        if (!part.empty()) {
            parts.emplace_back(part);
        }
        start = end + 1;
    }
    return parts;
}

std::vector<std::string> split_words(std::string_view text) {
    return split(text, " \t\r\n");
}

std::string join(const std::vector<std::string> &parts, std::string_view separator) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            out += separator;
        }
        out += parts[i];
    }
    return out;
}

std::string lowercase(std::string_view text) {
    std::string out(text);
    std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::optional<bool> parse_bool(std::string_view text) {
    auto value = lowercase(trim(text));
    if (value == "true" || value == "yes" || value == "on" || value == "1") {
        return true;
    }
    if (value == "false" || value == "no" || value == "off" || value == "0") {
        return false;
    }
    return std::nullopt;
}

std::optional<int> parse_int(std::string_view text) {
    text = trim(text);
    int value = 0;
    auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || end != text.data() + text.size() || text.empty()) {
        return std::nullopt;
    }
    return value;
}

std::optional<double> parse_double(std::string_view text) {
    text = trim(text);
    double value = 0;
    auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || end != text.data() + text.size() || text.empty()) {
        return std::nullopt;
    }
    return value;
}

std::size_t utf8_length(std::string_view text) {
    return static_cast<std::size_t>(std::ranges::count_if(text, [](unsigned char c) { return (c & 0xC0) != 0x80; }));
}

std::string utf8_first(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    std::size_t length = 1;
    while (length < text.size() && (static_cast<unsigned char>(text[length]) & 0xC0) == 0x80) {
        ++length;
    }
    return std::string(text.substr(0, length));
}

void utf8_pop_back(std::string &text) {
    while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0) == 0x80) {
        text.pop_back();
    }
    if (!text.empty()) {
        text.pop_back();
    }
}

void secure_clear(std::string &text) {
    if (!text.empty()) {
        explicit_bzero(text.data(), text.size());
    }
    text.clear();
}

} // namespace core
