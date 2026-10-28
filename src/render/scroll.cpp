#include "render/scroll.h"

#include <algorithm>

#include "config/scroll_config.h"
#include "config/style_config.h"

#include "render/sticker.h"

namespace render {

namespace {

void paint_roller(cairo_t *cr, const Box &roller, const Style &style) {
    const auto &ink = config::style::ink;
    double body_w = roller.w;
    double body_h = std::max(0.0, roller.h - style.shadow_offset);
    sticker(cr, roller, style.roller * 0.35, config::style::accent, ink, style.border_thick, style.shadow_offset, style.ring_width);
    double cap_w = std::min(style.cap, body_w / 2.0);
    double cap_h = body_h + style.cap_flare * 2.0;
    double cap_y = roller.y - style.cap_flare;
    rounded_box(cr, {roller.x, cap_y, cap_w, cap_h}, cap_w / 2.0, config::style::accent_alt, style.border_thin, ink);
    rounded_box(cr, {roller.x + body_w - cap_w, cap_y, cap_w, cap_h}, cap_w / 2.0, config::style::accent_alt, style.border_thin, ink);
    double strip_w = body_w - cap_w * 2.0 - 4.0 * style.scale;
    if (strip_w > 0.0) {
        auto highlight = mix(config::style::accent, config::style::white, config::scroll::highlight_mix);
        rounded_box(cr, {roller.x + cap_w + 2.0 * style.scale, roller.y + body_h * 0.22, strip_w, body_h * 0.18}, body_h * 0.09, highlight);
    }
}

} // namespace

ScrollGeometry scroll_geometry(const Box &panel, const Style &style) {
    double paper_height = panel.h;
    double left = panel.x - style.overhang;
    double width = panel.w + style.overhang * 2.0;
    ScrollGeometry geometry;
    geometry.paper = {panel.x, panel.y, panel.w, paper_height};
    geometry.first_roller = {left, panel.y - style.roller, width, style.roller};
    geometry.second_roller = {left, panel.y + paper_height, width, style.roller};
    return geometry;
}

Box scroll_bounds(const ScrollGeometry &geometry) {
    return geometry.first_roller.united(geometry.second_roller);
}

void paint_scroll(cairo_t *cr, const ScrollGeometry &geometry, const Style &style) {
    const auto &paper = geometry.paper;
    const auto &ink = config::style::ink;
    if (std::min(paper.w, paper.h) >= style.border_thick * 2.0) {
        sticker(cr, paper, style.radius_card, config::style::accent_container, ink, style.border_thick, style.shadow_offset, style.ring_width);
        Box inner{paper.x + style.band, paper.y + style.band, paper.w - style.band * 2.0, paper.h - style.shadow_offset - style.band * 2.0};
        if (inner.w > 0.0 && inner.h > 0.0) {
            rounded_box(cr, inner, std::max(0.0, style.radius_card - 2.0 * style.scale), config::style::paper, style.border_thin, ink);
        }
    }
    paint_roller(cr, geometry.first_roller, style);
    paint_roller(cr, geometry.second_roller, style);
}

} // namespace render
