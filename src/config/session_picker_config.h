#pragma once

#include "config/theme_config.h"

namespace config::session_picker {

// Layout
inline constexpr WidgetGeometry geometry{Anchor::Bottom, -130, -16, 240, 44};
inline constexpr int max_items = 8;
inline constexpr int item_height = 40;
inline constexpr int menu_gap = 4;
inline constexpr double corner_radius = 10.0;
inline constexpr double border_width = 5.0;
inline constexpr int indicator_size = 16;
inline constexpr int indicator_gap = 8;
inline constexpr bool show_type = false;

// Typography, in points
inline constexpr int font_size = 17;

// Colors
inline constexpr Color pill_color{0.0, 0.0, 0.0, 0.7};

// Text
inline constexpr const char *empty_text = "No sessions found";

} // namespace config::session_picker
