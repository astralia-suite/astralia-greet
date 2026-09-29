#pragma once

#include <format>
#include <string_view>
#include <utility>

namespace core {

enum class LogLevel { Info,
                      Warn,
                      Error };

void log(LogLevel level, std::string_view message);

template <typename... Args>
void info(std::format_string<Args...> format, Args &&...args) {
    log(LogLevel::Info, std::format(format, std::forward<Args>(args)...));
}

template <typename... Args>
void warn(std::format_string<Args...> format, Args &&...args) {
    log(LogLevel::Warn, std::format(format, std::forward<Args>(args)...));
}

template <typename... Args>
void error(std::format_string<Args...> format, Args &&...args) {
    log(LogLevel::Error, std::format(format, std::forward<Args>(args)...));
}

} // namespace core
