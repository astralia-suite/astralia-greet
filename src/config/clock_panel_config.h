#pragma once

#include "config/theme_config.h"

namespace config::clock_panel {

// Layout, covers the clock, date and hint geometries
inline constexpr WidgetGeometry geometry{Anchor::Center, 0, 0, 440, 546};
inline constexpr double corner_radius = 25.0;
inline constexpr double border_width = 2.0;

// Colors
inline constexpr Color fill{0.0, 0.0, 0.0, 0.7};

} // namespace config::clock_panel
