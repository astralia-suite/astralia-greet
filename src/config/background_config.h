#pragma once

#include "config/style_config.h"
#include "config/theme_config.h"

namespace config::background {

// Image
inline constexpr const char *image = ASTRALIA_GREET_DATA_DIR "/wallpaper.png";
inline constexpr const char *source_image = ASTRALIA_GREET_SOURCE_DIR "/assets/wallpaper.png";
inline constexpr BackgroundMode mode = BackgroundMode::Fill;
inline constexpr double dim = 0.0;

// Gradient, top to bottom
inline constexpr Color color = style::paper;
inline constexpr Color color2 = style::accent_container;

} // namespace config::background
