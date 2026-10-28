#include "modules/clock_panel.h"

#include "config/clock_panel_config.h"
#include "config/style_config.h"

#include "render/sticker.h"
#include "render/style.h"

namespace modules {

void ClockPanel::layout(int width, int height) {
    auto scale = render::ui_scale(width, height);
    rect_ = render::place(config::clock_panel::geometry, width, height, scale);
    auto style = render::make_style(scale);
    card_.reset(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, rect_.width, rect_.height));
    cairo_t *cr = cairo_create(card_.get());
    render::sticker(cr, {0.0, 0.0, static_cast<double>(rect_.width), static_cast<double>(rect_.height)}, style.radius_card, config::style::surface, config::style::ink, style.border_thick, style.shadow_offset, style.ring_width);
    cairo_destroy(cr);
}

render::Rect ClockPanel::bounds() const {
    return rect_;
}

bool ClockPanel::visible(const app::GreeterState &state) const {
    return state.view == app::View::Clock;
}

void ClockPanel::draw(cairo_t *cr, const app::GreeterState &state) {
    if (!visible(state) || !card_) {
        return;
    }
    cairo_set_source_surface(cr, card_.get(), rect_.x, rect_.y);
    cairo_paint(cr);
}

} // namespace modules
