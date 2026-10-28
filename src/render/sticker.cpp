#include "render/sticker.h"

#include <algorithm>

#include "config/style_config.h"

#include "render/canvas.h"

namespace render {

config::Color mix(const config::Color &from, const config::Color &to, double amount) {
    return {from.r + (to.r - from.r) * amount, from.g + (to.g - from.g) * amount, from.b + (to.b - from.b) * amount, from.a + (to.a - from.a) * amount};
}

void rounded_box(cairo_t *cr, const Box &box, double radius, const config::Color &fill, double border, const config::Color &outline) {
    rounded_rect(cr, box.x, box.y, box.w, box.h, radius);
    set_color(cr, fill);
    cairo_fill(cr);
    if (border > 0.0) {
        double inset = border / 2.0;
        rounded_rect(cr, box.x + inset, box.y + inset, box.w - border, box.h - border, radius - inset);
        set_color(cr, outline);
        cairo_set_line_width(cr, border);
        cairo_stroke(cr);
    }
}

void sticker(cairo_t *cr, const Box &box, double radius, const config::Color &fill, const config::Color &outline, double border, double shadow, double ring) {
    double width = std::max(0.0, box.w);
    double height = std::max(0.0, box.h - shadow);
    double corner = std::min(radius, std::min(width, height) / 2.0);
    if (shadow > 0.0) {
        rounded_box(cr, {box.x, box.y + shadow, width, height}, corner, outline);
    }
    const auto &paper = config::style::paper;
    config::Color rim{paper.r, paper.g, paper.b, paper.a * outline.a};
    bool same = fill.r == rim.r && fill.g == rim.g && fill.b == rim.b && fill.a == rim.a;
    if (ring <= 0.0 || same) {
        rounded_box(cr, {box.x, box.y, width, height}, corner, fill, border, outline);
        return;
    }
    rounded_box(cr, {box.x, box.y, width, height}, corner, rim, border, outline);
    double inner = border + ring;
    rounded_box(cr, {box.x + inner, box.y + inner, std::max(0.0, width - 2.0 * inner), std::max(0.0, height - 2.0 * inner)}, std::max(0.0, corner - inner), fill);
}

void selectable(cairo_t *cr, const Box &box, bool selected, bool hovered, const config::Color &idle, const Style &style) {
    const auto &ink = config::style::ink;
    if (selected) {
        double offset = style.shadow_widget;
        sticker(cr, {box.x + offset, box.y + offset, std::max(0.0, box.w - offset), std::max(0.0, box.h - offset)}, style.radius_widget, config::style::accent, ink, style.border_thin, 0.0, 0.0);
        return;
    }
    sticker(cr, box, style.radius_widget, hovered ? config::style::pop_a : idle, ink, style.border_thin, style.shadow_widget, 0.0);
}

} // namespace render
