#pragma once

#include "config/theme_config.h"

namespace config::hint {

// Layout
inline constexpr WidgetGeometry geometry{Anchor::Center, 0, 230, 400, 30};
inline constexpr bool show_layout = false;

// Typography, in points
inline constexpr int font_size = 15;

// Colors
inline constexpr Color warn_color{224 / 255.0, 168 / 255.0, 58 / 255.0, 1.0};

// Text
inline constexpr const char *caps_lock_text = "Caps Lock is on";
inline constexpr const char *idle_text = "Press any key to unlock";

} // namespace config::hint
