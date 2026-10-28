#pragma once

#include "config/theme_config.h"

namespace config::scroll {

// Scroll
inline constexpr double roller = 14.0;
inline constexpr double overhang = 6.0;
inline constexpr double cap = 6.0;
inline constexpr double band = 4.0;
inline constexpr double cap_flare = 2.0;
inline constexpr double highlight_mix = 0.35;

// Card
inline constexpr double width = 460.0;
inline constexpr double max_height = 640.0;
inline constexpr double padding = 20.0;
inline constexpr double header_height = 32.0;
inline constexpr double header_divider_gap = 4.0;
inline constexpr double header_tag_pad = 8.0;
inline constexpr double content_gap = 8.0;
inline constexpr double close_button = 24.0;
inline constexpr double gap_below_icon = 8.0;
inline constexpr double screen_margin = 12.0;
inline constexpr double toggle_width = 36.0;
inline constexpr double toggle_height = 20.0;

// Rows
inline constexpr double row_gap = 6.0;
inline constexpr double list_spacing = 5.0;
inline constexpr double row_icon_gap = 8.0;
inline constexpr double row_text_left = 40.0;
inline constexpr double device_row_height = 52.0;
inline constexpr double section_height = 22.0;
inline constexpr double banner_height = 48.0;
inline constexpr double empty_height = 72.0;
inline constexpr double action_gap = 4.0;
inline constexpr double icon_size = 18.0;

// Dialog
inline constexpr double dialog_spacer = 14.0;
inline constexpr double dialog_top_margin = 6.0;
inline constexpr double dialog_button = 28.0;
inline constexpr double dialog_button_pad = 16.0;
inline constexpr double field_height = 34.0;
inline constexpr double dot_size = 8.0;
inline constexpr double dot_gap = 4.0;

// Input
inline constexpr double wheel_pixels = 40.0;

// Typography, in points
inline constexpr int text_size = 13;
inline constexpr int title_size = 13;

} // namespace config::scroll
