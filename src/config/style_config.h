#pragma once

#include "config/theme_config.h"

namespace config::style {

// Palette, astralia-shell default theme: `theme_from_seed` with no wallpaper seeds (render/theme.cpp, hue 250); update together
inline constexpr Color ink{0.0000, 0.0887, 0.1830, 1.0};
inline constexpr Color paper{0.9485, 0.9704, 0.9940, 1.0};
inline constexpr Color surface{0.8908, 0.9342, 0.9810, 1.0};
inline constexpr Color surface_alt{0.7824, 0.8685, 0.9603, 1.0};
inline constexpr Color accent{0.4970, 0.6878, 0.8857, 1.0};
inline constexpr Color accent_alpha12{0.4970, 0.6878, 0.8857, 0.1216};
inline constexpr Color accent_alpha25{0.4970, 0.6878, 0.8857, 0.2510};
inline constexpr Color accent_container{0.7745, 0.8838, 1.0000, 1.0};
inline constexpr Color accent_alt{0.7387, 0.7118, 0.9347, 1.0};
inline constexpr Color pop_a{0.9312, 0.5768, 0.6316, 1.0};
inline constexpr Color pop_b{0.7349, 0.8731, 0.5987, 1.0};
inline constexpr Color lavender{0.1976, 0.3787, 0.5581, 1.0};
inline constexpr Color text{0.0000, 0.0887, 0.1830, 1.0};
inline constexpr Color text_alpha06{0.0000, 0.0887, 0.1830, 0.0588};
inline constexpr Color text_alpha08{0.0000, 0.0887, 0.1830, 0.0784};
inline constexpr Color text_alpha15{0.0000, 0.0887, 0.1830, 0.1490};
inline constexpr Color text_muted{0.1996, 0.3116, 0.4264, 1.0};
inline constexpr Color text_dim{0.1996, 0.3116, 0.4264, 0.8157};
inline constexpr Color critical{0.8288, 0.2322, 0.2111, 1.0};
inline constexpr Color critical_alpha15{0.8288, 0.2322, 0.2111, 0.1490};
inline constexpr Color warn{0.7915, 0.5397, 0.0642, 1.0};
inline constexpr Color window_backdrop{0.0433, 0.0901, 0.1378, 0.7020};
inline constexpr Color white{1.0, 1.0, 1.0, 1.0};

// Metrics, for the reference screen
inline constexpr double radius_card = 10.0;
inline constexpr double radius_widget = 5.0;
inline constexpr double border_thin = 2.0;
inline constexpr double border_thick = 3.0;
inline constexpr double shadow_offset = 4.0;
inline constexpr double shadow_widget = 2.0;
inline constexpr double ring_width = 3.0;

} // namespace config::style
