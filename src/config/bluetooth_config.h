#pragma once

#include "config/theme_config.h"

namespace config::bluetooth {

// Layout, the box sits left of `power` (assumes three power actions) and right of `network`
inline constexpr WidgetGeometry geometry{Anchor::TopRight, -144, 20, 32, 32};

// Text
inline constexpr const char *title = "Bluetooth";
inline constexpr const char *off_text = "Bluetooth is off";
inline constexpr const char *empty_text = "No paired devices";
inline constexpr const char *connected_label = "Connected";
inline constexpr const char *paired_label = "Paired";
inline constexpr const char *connected_subtitle = "Connected";
inline constexpr const char *paired_subtitle = "Paired";
inline constexpr const char *busy_text = "Connecting…";

} // namespace config::bluetooth
