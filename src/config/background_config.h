#pragma once

#include "config/theme_config.h"

namespace config::background {

// Image
inline constexpr const char *image = ASTRALIA_GREET_DATA_DIR "/wallpaper.png";
inline constexpr const char *source_image = ASTRALIA_GREET_SOURCE_DIR "/assets/wallpaper.png";
inline constexpr BackgroundMode mode = BackgroundMode::Fill;
inline constexpr double dim = 0.0;
inline constexpr double login_dim = 0.7;

// Gradient, top to bottom
inline constexpr Color color{10 / 255.0, 6 / 255.0, 20 / 255.0, 1.0};
inline constexpr Color color2{29 / 255.0, 17 / 255.0, 59 / 255.0, 1.0};

} // namespace config::background
