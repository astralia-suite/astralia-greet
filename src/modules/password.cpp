#include "modules/password.h"

#include <algorithm>
#include <filesystem>
#include <utility>

#include "config/password_config.h"
#include "config/style_config.h"

#include "core/log.h"
#include "core/strings.h"

#include "render/canvas.h"
#include "render/icons.h"
#include "render/image.h"
#include "render/sticker.h"
#include "render/text.h"

namespace modules {

Password::Password()
    : geometry_(config::password::geometry), font_(render::text_font(config::password::font_size)), mask_(config::password::mask), placeholder_(config::password::placeholder), busy_text_(config::password::busy_text) {
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
    style_ = render::make_style(scale_);
    icon_font_ = render::icon_font(render::scaled(config::password::icon_size, scale_));
    int size = render::scaled(config::password::button_size, scale_);
    button_ = {rect_.right() - (rect_.height - size) / 2 - size, rect_.y + (rect_.height - size) / 2, size, size};
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
    namespace colors = config::style;
    render::rounded_box(cr, render::to_box(rect_), style_.radius_card, colors::pop_b, render::line_width(config::password::border_width, scale_), colors::ink);
    render::draw_text(cr, render::icon::lock, icon_font_, colors::text_muted, {rect_.x, rect_.y, rect_.height, rect_.height}, render::TextAlign::Center);
    bool has_text = !state.password.empty();
    render::selectable(cr, render::to_box(button_), has_text, hovered_, colors::surface_alt, style_);
    render::draw_text(cr, render::icon::arrow_right, icon_font_, has_text ? colors::text : colors::text_muted, {button_.x, button_.y, button_.width, button_.height - static_cast<int>(style_.shadow_widget)}, render::TextAlign::Center);

    render::Rect box{rect_.x + rect_.height, rect_.y, rect_.width - 2 * rect_.height, rect_.height};
    if (state.busy) {
        render::draw_text(cr, busy_text_, font_, colors::text_muted, box, render::TextAlign::Center);
        return;
    }
    if (!state.message.empty()) {
        if (state.message_is_error) {
            render::draw_text(cr, config::password::error_text, font_, colors::critical, box, render::TextAlign::Center);
        } else {
            render::draw_text(cr, state.message, font_, colors::text_muted, box, render::TextAlign::Center);
        }
        return;
    }
    if (!has_text) {
        render::draw_text(cr, placeholder_, font_, colors::text_muted, box, render::TextAlign::Center);
        return;
    }
    auto count = core::utf8_length(state.password);
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
        return;
    }
    std::string masked;
    for (std::size_t i = 0; i < count; ++i) {
        masked += mask_;
    }
    render::draw_text(cr, masked, font_, colors::text, box, render::TextAlign::Center);
}

bool Password::click(int x, int y, app::GreeterState &state) {
    if (!visible(state)) {
        return false;
    }
    state.focus = app::Focus::Password;
    if (button_.contains(x, y) && !state.password.empty() && !state.busy) {
        state.request = app::Request::Login;
    }
    return true;
}

bool Password::hover(int x, int y, const app::GreeterState &state) {
    bool inside = visible(state) && button_.contains(x, y);
    return std::exchange(hovered_, inside) != inside;
}

} // namespace modules
