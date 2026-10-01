#include "render/layout.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace render {

namespace {

int column(config::Anchor anchor) {
    switch (anchor) {
    case config::Anchor::TopLeft:
    case config::Anchor::Left:
    case config::Anchor::BottomLeft:
        return 0;
    case config::Anchor::Top:
    case config::Anchor::Center:
    case config::Anchor::Bottom:
        return 1;
    default:
        return 2;
    }
}

int row(config::Anchor anchor) {
    switch (anchor) {
    case config::Anchor::TopLeft:
    case config::Anchor::Top:
    case config::Anchor::TopRight:
        return 0;
    case config::Anchor::Left:
    case config::Anchor::Center:
    case config::Anchor::Right:
        return 1;
    default:
        return 2;
    }
}

int align(int slot, int available, int size) {
    if (slot == 0) {
        return 0;
    }
    if (slot == 1) {
        return (available - size) / 2;
    }
    return available - size;
}

} // namespace

bool Rect::empty() const {
    return width <= 0 || height <= 0;
}

int Rect::right() const {
    return x + width;
}

int Rect::bottom() const {
    return y + height;
}

double Rect::center_x() const {
    return x + width / 2.0;
}

double Rect::center_y() const {
    return y + height / 2.0;
}

bool Rect::contains(int px, int py) const {
    return px >= x && py >= y && px < right() && py < bottom();
}

bool Rect::intersects(const Rect &other) const {
    return !intersected(other).empty();
}

Rect Rect::intersected(const Rect &other) const {
    int left = std::max(x, other.x);
    int top = std::max(y, other.y);
    int r = std::min(right(), other.right());
    int b = std::min(bottom(), other.bottom());
    return {left, top, std::max(0, r - left), std::max(0, b - top)};
}

Rect Rect::united(const Rect &other) const {
    if (empty()) {
        return other;
    }
    if (other.empty()) {
        return *this;
    }
    int left = std::min(x, other.x);
    int top = std::min(y, other.y);
    return {left, top, std::max(right(), other.right()) - left, std::max(bottom(), other.bottom()) - top};
}

Rect Rect::padded(int amount) const {
    return {x - amount, y - amount, width + amount * 2, height + amount * 2};
}

std::vector<Rect> merge_overlapping(std::vector<Rect> rects) {
    std::erase_if(rects, [](const Rect &rect) { return rect.empty(); });
    bool merged = true;
    while (merged) {
        merged = false;
        for (std::size_t i = 0; i < rects.size() && !merged; ++i) {
            for (std::size_t j = i + 1; j < rects.size(); ++j) {
                if (rects[i].intersects(rects[j])) {
                    rects[i] = rects[i].united(rects[j]);
                    rects.erase(rects.begin() + static_cast<std::ptrdiff_t>(j));
                    merged = true;
                    break;
                }
            }
        }
    }
    return rects;
}

double ui_scale(int screen_width, int screen_height) {
    return std::min(static_cast<double>(screen_width) / config::reference_width, static_cast<double>(screen_height) / config::reference_height);
}

int scaled(int value, double scale) {
    return static_cast<int>(std::lround(value * scale));
}

double scaled(double value, double scale) {
    return value * scale;
}

double line_width(double width, double scale) {
    return std::max(1.0, width * scale);
}

Rect place(const config::WidgetGeometry &geometry, int screen_width, int screen_height, double scale) {
    int width = scaled(geometry.width, scale);
    int height = scaled(geometry.height, scale);
    int x = align(column(geometry.anchor), screen_width, width);
    int y = align(row(geometry.anchor), screen_height, height);
    return {x + scaled(geometry.x, scale), y + scaled(geometry.y, scale), width, height};
}

Placement fit_image(int image_width, int image_height, int area_width, int area_height, config::BackgroundMode mode) {
    Placement placement;
    if (image_width <= 0 || image_height <= 0) {
        return placement;
    }
    double sx = static_cast<double>(area_width) / image_width;
    double sy = static_cast<double>(area_height) / image_height;
    switch (mode) {
    case config::BackgroundMode::Fill:
        placement.scale_x = placement.scale_y = std::max(sx, sy);
        break;
    case config::BackgroundMode::Fit:
        placement.scale_x = placement.scale_y = std::min(sx, sy);
        break;
    case config::BackgroundMode::Stretch:
        placement.scale_x = sx;
        placement.scale_y = sy;
        return placement;
    case config::BackgroundMode::Tile:
        return placement;
    case config::BackgroundMode::Center:
        break;
    }
    placement.x = (area_width - image_width * placement.scale_x) / 2.0;
    placement.y = (area_height - image_height * placement.scale_y) / 2.0;
    return placement;
}

} // namespace render
