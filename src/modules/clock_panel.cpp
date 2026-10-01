#include "modules/clock_panel.h"

#include "config/clock_panel_config.h"

#include "render/canvas.h"

namespace modules {

void ClockPanel::layout(int width, int height) {
    scale_ = render::ui_scale(width, height);
    rect_ = render::place(config::clock_panel::geometry, width, height, scale_);
}

render::Rect ClockPanel::bounds() const {
    return rect_;
}

bool ClockPanel::visible(const app::GreeterState &state) const {
    return state.view == app::View::Clock;
}

void ClockPanel::draw(cairo_t *cr, const app::GreeterState &state) {
    namespace defaults = config::clock_panel;
    if (!visible(state)) {
        return;
    }
    double border = render::line_width(defaults::border_width, scale_);
    double inset = border / 2.0;
    render::rounded_rect(cr, rect_.x + inset, rect_.y + inset, rect_.width - inset * 2, rect_.height - inset * 2, render::scaled(defaults::corner_radius, scale_));
    render::set_color(cr, defaults::fill);
    cairo_fill_preserve(cr);
    render::set_color(cr, config::accent);
    cairo_set_line_width(cr, border);
    cairo_stroke(cr);
}

} // namespace modules
