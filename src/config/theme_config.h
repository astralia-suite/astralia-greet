#pragma once

namespace config {

struct Color {
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    double a = 1.0;
};

enum class Anchor { TopLeft,
                    Top,
                    TopRight,
                    Left,
                    Center,
                    Right,
                    BottomLeft,
                    Bottom,
                    BottomRight };

enum class BackgroundMode { Fill,
                            Fit,
                            Center,
                            Tile,
                            Stretch };

struct WidgetGeometry {
    Anchor anchor = Anchor::Center;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

// Palette, mirrors astralia-shell-i3/src/core/palette.h: text, text_alpha65, accent, critical, text_alpha08, text_alpha20
inline constexpr Color fg{240 / 255.0, 236 / 255.0, 249 / 255.0, 1.0};
inline constexpr Color muted{240 / 255.0, 236 / 255.0, 249 / 255.0, 166 / 255.0};
inline constexpr Color accent{155 / 255.0, 87 / 255.0, 244 / 255.0, 1.0};
inline constexpr Color error{244 / 255.0, 71 / 255.0, 71 / 255.0, 1.0};
inline constexpr Color field{240 / 255.0, 236 / 255.0, 249 / 255.0, 20 / 255.0};
inline constexpr Color field_border{240 / 255.0, 236 / 255.0, 249 / 255.0, 51 / 255.0};

// Font
inline constexpr const char *font_family = "Comic Shanns Mono";
inline constexpr int font_size = 12;

} // namespace config
