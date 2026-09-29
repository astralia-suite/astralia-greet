#pragma once

#include "config/theme_config.h"

namespace config::password {

// Layout
inline constexpr WidgetGeometry geometry{Anchor::Center, 0, 0, 300, 46};
inline constexpr double border_width = 1.5;
inline constexpr double focus_border_width = 2.0;
inline constexpr int caret_gap = 3;

// Echo
inline constexpr const char *echo_image = ASTRALIA_GREET_DATA_DIR "/electro.png";
inline constexpr const char *source_echo_image = ASTRALIA_GREET_SOURCE_DIR "/assets/electro.png";
inline constexpr int echo_size = 18;

// Typography, in points
inline constexpr int font_size = 13;

// Text
inline constexpr const char *mask = "•";
inline constexpr const char *placeholder = "Password";
inline constexpr const char *busy_text = "Authenticating…";
inline constexpr const char *error_text = "Skill Issue";

} // namespace config::password
