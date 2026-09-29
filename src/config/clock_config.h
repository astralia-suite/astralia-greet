#pragma once

#include "config/theme_config.h"

namespace config::clock {

// Layout
inline constexpr WidgetGeometry geometry{Anchor::Center, 0, -58, 400, 380};

// Format
inline constexpr const char *hour_format = "%H";
inline constexpr const char *minute_format = "%M";

// Typography, in points
inline constexpr int time_size = 140;

} // namespace config::clock
