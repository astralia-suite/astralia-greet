#pragma once

#include "config/theme_config.h"

namespace config::date {

// Layout
inline constexpr WidgetGeometry geometry{Anchor::Center, 0, 172, 400, 44};

// Format
inline constexpr const char *format = "%A, %B %-d";

// Typography, in points
inline constexpr int font_size = 24;

} // namespace config::date
