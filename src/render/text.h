#pragma once

#include <cairo.h>
#include <string>
#include <string_view>

#include "config/theme_config.h"

#include "render/layout.h"

namespace render {

enum class TextAlign { Left,
                       Center,
                       Right };

struct TextSize {
    int width = 0;
    int height = 0;
};

std::string text_font(int size, bool bold = false);
std::string icon_font(int pixel_size);
TextSize measure_text(cairo_t *cr, std::string_view text, std::string_view font);
void draw_text(cairo_t *cr, std::string_view text, std::string_view font, const config::Color &color, const Rect &box, TextAlign align);

} // namespace render
