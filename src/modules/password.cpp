#include "modules/password.h"

#include <algorithm>
#include <filesystem>

#include "config/password_config.h"

#include "core/log.h"
#include "core/strings.h"

#include "render/canvas.h"
#include "render/image.h"
#include "render/text.h"

namespace modules {

Password::Password()
    : geometry_(config::password::geometry), font_(render::text_font(config::password::font_size)), mask_(config::password::mask), placeholder_(config::password::placeholder), busy_text_(config::password::busy_text), text_color_(config::fg), fill_(config::field), border_(config::field_border), focus_border_(config::accent), muted_(config::muted) {
    std::error_code ec;
    std::filesystem::path path = std::filesystem::exists(config::password::echo_image, ec) ? config::password::echo_image : config::password::source_echo_image;
    echo_image_ = render::load_image(path);
    if (!echo_image_) {
        core::warn("cannot load password echo image {}", path.string());
    }
}

void Password::layout(int width, int height) {
    scale_ = render::ui_scale(width, height);
    rect_ = render::place(geometry_, width, height, scale_);
    echo_size_ = std::max(1, render::scaled(config::password::echo_size, scale_));
    if (echo_image_) {
        echo_ = render::scaled_image(echo_image_.get(), echo_size_);
    }
}

render::Rect Password::bounds() const {
    return rect_;
}

bool Password::visible(const app::GreeterState &state) const {
    return state.view == app::View::Login;
}

void Password::draw(cairo_t *cr, const app::GreeterState &state) {
    if (!visible(state)) {
        return;
    }
    bool focused = state.focus == app::Focus::Password;
    double focus_border = render::line_width(config::password::focus_border_width, scale_);
    double inset = focus_border;
    render::rounded_rect(cr, rect_.x + inset, rect_.y + inset, rect_.width - inset * 2, rect_.height - inset * 2, rect_.height / 2.0 - inset);
    render::set_color(cr, fill_);
    cairo_fill_preserve(cr);
    render::set_color(cr, focused ? focus_border_ : border_);
    cairo_set_line_width(cr, focused ? focus_border : render::line_width(config::password::border_width, scale_));
    cairo_stroke(cr);

    auto box = rect_.padded(-render::scaled(12, scale_));
    if (state.busy) {
        render::draw_text(cr, busy_text_, font_, muted_, box, render::TextAlign::Center);
        return;
    }
    if (!state.message.empty()) {
        if (state.message_is_error) {
            render::draw_text(cr, config::password::error_text, font_, config::error, box, render::TextAlign::Center);
        } else {
            render::draw_text(cr, state.message, font_, muted_, box, render::TextAlign::Center);
        }
        return;
    }
    if (state.password.empty()) {
        render::draw_text(cr, placeholder_, font_, muted_, box, render::TextAlign::Center);
        return;
    }
    auto count = core::utf8_length(state.password);
    double row_end = 0.0;
    double row_height = 0.0;
    if (echo_) {
        int size = echo_size_;
        auto shown = std::min(count, static_cast<std::size_t>(std::max(1, box.width / size)));
        double width = static_cast<double>(shown * size);
        double x = box.center_x() - width / 2.0;
        double y = rect_.center_y() - size / 2.0;
        for (std::size_t i = 0; i < shown; ++i) {
            cairo_set_source_surface(cr, echo_.get(), x + static_cast<double>(i * size), y);
            cairo_paint(cr);
        }
        row_end = x + width;
        row_height = size;
    } else {
        std::string masked;
        for (std::size_t i = 0; i < count; ++i) {
            masked += mask_;
        }
        render::draw_text(cr, masked, font_, text_color_, box, render::TextAlign::Center);
        auto size = render::measure_text(cr, masked, font_);
        row_end = box.center_x() + size.width / 2.0;
        row_height = size.height;
    }
    if (focused) {
        double x = std::min(row_end + render::scaled(config::password::caret_gap, scale_), static_cast<double>(box.right()));
        render::set_color(cr, focus_border_);
        cairo_set_line_width(cr, render::line_width(1.5, scale_));
        cairo_move_to(cr, x, rect_.center_y() - row_height / 2.0);
        cairo_line_to(cr, x, rect_.center_y() + row_height / 2.0);
        cairo_stroke(cr);
    }
}

bool Password::click(int, int, app::GreeterState &state) {
    if (!visible(state)) {
        return false;
    }
    state.focus = app::Focus::Password;
    return true;
}

} // namespace modules
