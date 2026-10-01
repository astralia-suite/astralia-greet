#pragma once

#include <vector>

#include "config/theme_config.h"

namespace render {

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool empty() const;
    int right() const;
    int bottom() const;
    double center_x() const;
    double center_y() const;
    bool contains(int px, int py) const;
    bool intersects(const Rect &other) const;
    Rect intersected(const Rect &other) const;
    Rect united(const Rect &other) const;
    Rect padded(int amount) const;
};

struct Placement {
    double scale_x = 1.0;
    double scale_y = 1.0;
    double x = 0.0;
    double y = 0.0;
};

std::vector<Rect> merge_overlapping(std::vector<Rect> rects);
double ui_scale(int screen_width, int screen_height);
int scaled(int value, double scale);
double scaled(double value, double scale);
double line_width(double width, double scale);
Rect place(const config::WidgetGeometry &geometry, int screen_width, int screen_height, double scale);
Placement fit_image(int image_width, int image_height, int area_width, int area_height, config::BackgroundMode mode);

} // namespace render
