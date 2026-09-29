#pragma once

#include "config/theme_config.h"

namespace config::user_menu {

// Layout
inline constexpr WidgetGeometry geometry{Anchor::Bottom, 110, -16, 200, 32};
inline constexpr int max_items = 8;
inline constexpr int item_height = 30;
inline constexpr int menu_gap = 4;
inline constexpr double corner_radius = 10.0;
inline constexpr double border_width = 1.0;
inline constexpr int indicator_size = 16;
inline constexpr int indicator_gap = 8;

// Typography, in points
inline constexpr int font_size = 14;

// Colors
inline constexpr Color pill_color{0.0, 0.0, 0.0, 0.7};

} // namespace config::user_menu
