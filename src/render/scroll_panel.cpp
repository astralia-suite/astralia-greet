#include "render/scroll_panel.h"

#include <algorithm>
#include <cmath>

#include "config/style_config.h"

#include "render/icons.h"
#include "render/scroll.h"
#include "render/sticker.h"
#include "render/text.h"
#include "render/widgets.h"

namespace render {

namespace {

SurfacePtr make_surface(double width, double height) {
    int w = std::max(1, static_cast<int>(std::ceil(width)));
    int h = std::max(1, static_cast<int>(std::ceil(height)));
    return SurfacePtr(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h));
}

constexpr double body_lookahead = 2.0;

} // namespace

ScrollPanel::ScrollPanel(const PanelContent &content) : content_(content) {}

void ScrollPanel::layout(const Style &style, const Rect &icon, int screen_width, int screen_height) {
    style_ = style;
    icon_ = icon;
    screen_width_ = screen_width;
    screen_height_ = screen_height;
    if (open_) {
        rebuild();
    }
}

void ScrollPanel::open() {
    open_ = true;
    scroll_ = 0.0;
    rebuild();
}

void ScrollPanel::close() {
    open_ = false;
    chrome_.reset();
    body_.reset();
    shell_.reset();
}

void ScrollPanel::invalidate() {
    if (open_) {
        rebuild();
    }
}

double ScrollPanel::max_scroll() const {
    return std::max(0.0, content_height_ - view_height_);
}

void ScrollPanel::place() {
    double line = std::max(1.0, std::round(style_.scale));
    view_top_ = style_.padding + style_.header_height + style_.header_divider_gap + line + style_.content_gap;
    content_height_ = content_.content_height(style_);
    double top = std::round(icon_.bottom() + style_.gap_below_icon + style_.roller);
    double room = screen_height_ - top - style_.roller - style_.shadow_offset - style_.screen_margin - content_.dialog_reserve(style_);
    double full = std::round(std::min({style_.max_height, room, view_top_ + content_height_ + style_.padding}));
    view_height_ = std::max(0.0, full - style_.padding - view_top_);
    double margin = style_.screen_margin + style_.overhang;
    double left = std::clamp(static_cast<double>(icon_.right()) - style_.width, margin, std::max(margin, screen_width_ - style_.width - margin));
    card_ = {std::round(left), top, std::round(style_.width), full};
    scroll_ = std::clamp(scroll_, 0.0, max_scroll());
}

void ScrollPanel::paint_chrome(cairo_t *cr) {
    header_regions_.clear();
    PanelPaint paint(header_regions_);
    const auto &ink = config::style::ink;
    auto title = content_.title();
    auto size = measure_text(cr, title, style_.title_font);
    double title_y = style_.padding + (style_.header_height - size.height) / 2.0;
    double tag_pad = style_.header_tag_pad;
    sticker(cr, {style_.padding - tag_pad, title_y - tag_pad / 2.0, size.width + 2.0 * tag_pad, size.height + tag_pad + style_.shadow_widget}, style_.radius_widget, content_.tint(), ink, style_.border_thin, style_.shadow_widget, 0.0);
    draw_text(cr, title, style_.title_font, config::style::text, Box{style_.padding, title_y, size.width + 4.0, static_cast<double>(size.height)}.bounding_rect(), TextAlign::Left);

    Box close{card_.w - style_.padding - style_.close_button, style_.padding + (style_.header_height - style_.close_button) / 2.0, style_.close_button, style_.close_button};
    widgets::icon_button(cr, close, icon::close, config::style::text, style_);
    paint.region(close, region_close);
    double left = close.x;
    if (content_.has_toggle()) {
        Box toggle{close.x - style_.row_gap - style_.toggle_width, style_.padding + (style_.header_height - style_.toggle_height) / 2.0, style_.toggle_width, style_.toggle_height};
        widgets::toggle(cr, toggle, content_.toggle_on(), style_);
        paint.region(toggle, region_toggle);
        left = toggle.x;
    }
    if (const char *glyph = content_.header_action()) {
        Box action{left - style_.row_gap - style_.close_button, close.y, style_.close_button, style_.close_button};
        widgets::icon_button(cr, action, glyph, config::style::text, style_);
        paint.region(action, region_action);
    }
    double divider = style_.padding + style_.header_height + style_.header_divider_gap;
    double line = view_top_ - style_.content_gap - divider;
    rounded_box(cr, {style_.padding, divider, card_.w - 2.0 * style_.padding, line}, 0.0, ink);
}

void ScrollPanel::rebuild() {
    place();
    shell_.reset();
    chrome_ = make_surface(card_.w, view_top_);
    if (cairo_t *cr = cairo_create(chrome_.get()); cr) {
        style_.text_height = measure_text(cr, "Ag", style_.text_font).height;
        paint_chrome(cr);
        cairo_destroy(cr);
    }
    body_ = make_surface(card_.w - 2.0 * style_.padding, content_height_);
    body_regions_.clear();
    painted_to_ = 0.0;
    paint_body_to(scroll_ + view_height_ * body_lookahead);
}

void ScrollPanel::paint_body_to(double bottom) {
    bottom = std::min(bottom, content_height_);
    if (!body_ || bottom <= painted_to_) {
        return;
    }
    if (cairo_t *cr = cairo_create(body_.get()); cr) {
        PanelPaint paint(body_regions_);
        content_.paint(cr, {0.0, painted_to_, card_.w - 2.0 * style_.padding, bottom - painted_to_}, style_, paint);
        cairo_destroy(cr);
    }
    painted_to_ = bottom;
}

void ScrollPanel::build_shell() {
    Box bounds = scroll_bounds(scroll_geometry(card_, style_));
    double left = std::floor(bounds.x);
    double top = std::floor(bounds.y);
    shell_origin_ = {left, top, 0.0, 0.0};
    shell_ = make_surface(std::ceil(bounds.x + bounds.w) - left, std::ceil(bounds.y + bounds.h) - top);
    if (cairo_t *cr = cairo_create(shell_.get()); cr) {
        cairo_translate(cr, -left, -top);
        paint_scroll(cr, scroll_geometry(card_, style_), style_);
        cairo_set_source_surface(cr, chrome_.get(), card_.x, card_.y);
        cairo_paint(cr);
        cairo_destroy(cr);
    }
}

void ScrollPanel::paint(cairo_t *cr) {
    if (!open_ || !chrome_) {
        return;
    }
    if (!shell_) {
        build_shell();
    }
    cairo_set_source_surface(cr, shell_.get(), shell_origin_.x, shell_origin_.y);
    cairo_paint(cr);
    cairo_save(cr);
    cairo_rectangle(cr, card_.x + style_.padding, card_.y + view_top_, card_.w - 2.0 * style_.padding, view_height_);
    cairo_clip(cr);
    cairo_set_source_surface(cr, body_.get(), card_.x + style_.padding, card_.y + view_top_ - scroll_);
    cairo_paint(cr);
    cairo_restore(cr);
    dialog_regions_.clear();
    Box area = dialog_area();
    if (area.h > 0.0) {
        cairo_save(cr);
        cairo_translate(cr, area.x, area.y);
        PanelPaint paint(dialog_regions_);
        content_.paint_dialog(cr, {0.0, 0.0, area.w, area.h}, style_, paint);
        cairo_restore(cr);
    }
}

Box ScrollPanel::dialog_area() const {
    double height = content_.dialog_height(style_);
    if (!open_ || height <= 0.0) {
        return {};
    }
    return {card_.x, card_.y + card_.h + style_.roller + style_.shadow_offset + style_.gap_below_icon, card_.w, height};
}

std::optional<PanelRegion> ScrollPanel::press(double x, double y) const {
    if (!open_) {
        return std::nullopt;
    }
    if (Box area = dialog_area(); area.contains(x, y)) {
        for (const auto &region : dialog_regions_) {
            if (region.box.contains(x - area.x, y - area.y)) {
                return region;
            }
        }
        return std::nullopt;
    }
    double local_x = x - card_.x;
    double local_y = y - card_.y;
    for (const auto &region : header_regions_) {
        if (region.box.contains(local_x, local_y)) {
            return region;
        }
    }
    Box view{style_.padding, view_top_, card_.w - 2.0 * style_.padding, view_height_};
    if (!view.contains(local_x, local_y)) {
        return std::nullopt;
    }
    double body_x = local_x - style_.padding;
    double body_y = local_y - view_top_ + scroll_;
    for (const auto &region : body_regions_) {
        if (region.box.contains(body_x, body_y)) {
            return region;
        }
    }
    return std::nullopt;
}

bool ScrollPanel::wheel(double pixels) {
    if (!open_) {
        return false;
    }
    double next = std::clamp(scroll_ + pixels, 0.0, max_scroll());
    if (next == scroll_) {
        return false;
    }
    scroll_ = next;
    paint_body_to(scroll_ + view_height_ * body_lookahead);
    return true;
}

bool ScrollPanel::visible() const {
    return open_;
}

bool ScrollPanel::contains(double x, double y) const {
    return open_ && (scroll_bounds(scroll_geometry(card_, style_)).contains(x, y) || dialog_area().contains(x, y));
}

Box ScrollPanel::extent() const {
    return scroll_bounds(scroll_geometry(card_, style_)).united(dialog_area());
}

double ScrollPanel::scroll_offset() const {
    return scroll_;
}

} // namespace render
