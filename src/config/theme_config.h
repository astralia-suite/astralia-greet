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

// Reference screen, every size in src/config/ is designed for it and scaled to the real screen
inline constexpr int reference_width = 1920;
inline constexpr int reference_height = 1200;

// Font
inline constexpr const char *font_family = "Comic Shanns Mono";
inline constexpr int font_size = 12;

} // namespace config
