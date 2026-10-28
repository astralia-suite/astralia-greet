#pragma once

#include "config/theme_config.h"

namespace config::network {

// Layout, the box sits left of `bluetooth` and `power` (assumes three power actions)
inline constexpr WidgetGeometry geometry{Anchor::TopRight, -184, 20, 32, 32};

// Text
inline constexpr const char *title = "Network";
inline constexpr const char *wired_text = "Ethernet";
inline constexpr const char *no_adapter_text = "No Wi-Fi adapter found";
inline constexpr const char *disabled_text = "WiFi is disabled";
inline constexpr const char *empty_text = "No networks in range";
inline constexpr const char *connected_label = "Connected";
inline constexpr const char *known_label = "Known";
inline constexpr const char *available_label = "Available";
inline constexpr const char *connected_subtitle = "Connected";
inline constexpr const char *saved_subtitle = "Saved";
inline constexpr const char *unsupported_subtitle = "Not supported";
inline constexpr const char *busy_text = "Connecting…";
inline constexpr const char *password_placeholder = "Password…";
inline constexpr const char *connect_text = "Connect";
inline constexpr const char *cancel_text = "Cancel";
inline constexpr int psk_min_length = 8;

} // namespace config::network
