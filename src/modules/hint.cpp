#include "modules/hint.h"

#include "config/hint_config.h"

namespace modules {

Hint::Hint()
    : geometry_(config::hint::geometry), font_(render::text_font(config::hint::font_size)), caps_lock_text_(config::hint::caps_lock_text), idle_text_(config::hint::idle_text), show_layout_(config::hint::show_layout), align_(render::TextAlign::Center), info_color_(config::muted), warn_color_(config::hint::warn_color), error_color_(config::error) {}

void Hint::layout(int width, int height) {
    rect_ = render::place(geometry_, width, height, render::ui_scale(width, height));
}

render::Rect Hint::bounds() const {
    return rect_;
}

bool Hint::visible(const app::GreeterState &state) const {
    return state.view == app::View::Clock;
}

void Hint::draw(cairo_t *cr, const app::GreeterState &state) {
    if (!visible(state)) {
        return;
    }
    std::string text;
    config::Color color = info_color_;
    if (!state.message.empty()) {
        text = state.message;
        color = state.message_is_error ? error_color_ : info_color_;
    } else if (state.caps_lock) {
        text = caps_lock_text_;
        color = warn_color_;
    } else {
        text = idle_text_;
    }
    if (show_layout_ && !state.layout.empty()) {
        text = text.empty() ? state.layout : text + "  ·  " + state.layout;
    }
    render::draw_text(cr, text, font_, color, rect_, align_);
}

} // namespace modules
