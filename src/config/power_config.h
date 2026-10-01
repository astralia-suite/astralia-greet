#pragma once

#include "config/theme_config.h"

namespace config::power {

// Layout
inline constexpr WidgetGeometry geometry{Anchor::TopRight, -28, 28, 0, 0};
inline constexpr int icon_size = 18;
inline constexpr int spacing = 16;

// Colors
inline constexpr Color hover_color{1.0, 1.0, 1.0, 1.0};

} // namespace config::power
