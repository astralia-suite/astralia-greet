#pragma once

#include <cstdint>

namespace render {

struct Hotspot {
    int x = 0;
    int y = 0;
};

Hotspot draw_cursor(std::uint8_t *data, int size, int stride);

} // namespace render
