#pragma once

#include <cairo.h>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "config/theme_config.h"

#include "render/layout.h"

namespace render {

struct Dropdown {
    std::string font;
    std::string indicator_font;
    int indicator_size = 0;
    int indicator_gap = 0;
    int max_items = 0;
    int item_height = 0;
    int menu_gap = 0;
    double corner_radius = 0.0;
    double border_width = 0.0;
    config::Color color;
    config::Color focus_color;
    config::Color menu_border;
    config::Color pill_color;

    std::size_t visible_count(std::size_t total) const;
    std::size_t first_visible(std::size_t index, std::size_t total) const;
    Rect menu_rect(const Rect &button, std::size_t total, int screen_height) const;
    Rect bounds(const Rect &button, bool open, std::size_t total, int screen_height) const;
    void draw_button(cairo_t *cr, const Rect &button, std::string_view label, const config::Color &color) const;
    void draw_menu(cairo_t *cr, const Rect &menu, const std::vector<std::string> &labels, std::size_t index) const;
    std::optional<std::size_t> hit(const Rect &menu, int x, int y, std::size_t index, std::size_t total) const;
};

} // namespace render
