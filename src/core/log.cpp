#include "core/log.h"

#include <cstdio>

namespace core {

void log(LogLevel level, std::string_view message) {
    int priority = 6;
    switch (level) {
    case LogLevel::Info:
        priority = 6;
        break;
    case LogLevel::Warn:
        priority = 4;
        break;
    case LogLevel::Error:
        priority = 3;
        break;
    }
    std::fprintf(stderr, "<%d>%.*s\n", priority, static_cast<int>(message.size()), message.data());
}

} // namespace core
