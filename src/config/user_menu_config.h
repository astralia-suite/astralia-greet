#pragma once

#include "config/theme_config.h"

namespace config::user_menu {

// Layout
inline constexpr WidgetGeometry geometry{Anchor::Bottom, 130, -16, 240, 44};
inline constexpr int max_items = 8;
inline constexpr int item_height = 40;
inline constexpr int menu_gap = 4;
inline constexpr int row_gap = 4;
inline constexpr int indicator_size = 16;
inline constexpr int indicator_gap = 8;

// Typography, in points
inline constexpr int font_size = 17;

} // namespace config::user_menu
