#include "modules/power.h"

#include <algorithm>
#include <utility>

#include "config/power_config.h"
#include "config/style_config.h"

#include "render/icons.h"
#include "render/text.h"
#include "render/widgets.h"

namespace modules {

Power::Power(const config::PowerSettings &power) {
    namespace defaults = config::power;
    if (power.allow_suspend) {
        actions_.push_back(app::Request::Suspend);
    }
    if (power.allow_reboot) {
        actions_.push_back(app::Request::Reboot);
    }
    if (power.allow_poweroff) {
        actions_.push_back(app::Request::PowerOff);
    }
    auto count = static_cast<int>(actions_.size());
    geometry_ = defaults::geometry;
    geometry_.width = count * defaults::box_size + std::max(0, count - 1) * defaults::spacing;
    geometry_.height = defaults::box_size;
}

void Power::layout(int width, int height) {
    auto scale = render::ui_scale(width, height);
    rect_ = render::place(geometry_, width, height, scale);
    icon_size_ = render::scaled(config::power::box_size, scale);
    spacing_ = render::scaled(config::power::spacing, scale);
    style_ = render::make_style(scale);
}

render::Rect Power::bounds() const {
    return rect_;
}

bool Power::visible(const app::GreeterState &state) const {
    return state.view == app::View::Clock;
}

render::Rect Power::slot(std::size_t index) const {
    return {rect_.x + static_cast<int>(index) * (icon_size_ + spacing_), rect_.y, icon_size_, icon_size_};
}

void Power::draw(cairo_t *cr, const app::GreeterState &state) {
    if (!visible(state)) {
        return;
    }
    for (std::size_t i = 0; i < actions_.size(); ++i) {
        const char *icon = nullptr;
        switch (actions_[i]) {
        case app::Request::Suspend:
            icon = render::icon::moon;
            break;
        case app::Request::Reboot:
            icon = render::icon::refresh;
            break;
        case app::Request::PowerOff:
            icon = render::icon::power;
            break;
        default:
            break;
        }
        if (icon) {
            render::widgets::icon_button(cr, render::to_box(slot(i)), icon, config::style::text, style_, hovered_ == i);
        }
    }
}

bool Power::click(int x, int y, app::GreeterState &state) {
    if (!visible(state)) {
        return false;
    }
    if (auto index = slot_at(x, y)) {
        state.request = actions_[*index];
        return true;
    }
    return false;
}

bool Power::hover(int x, int y, const app::GreeterState &state) {
    auto index = visible(state) ? slot_at(x, y) : std::nullopt;
    return std::exchange(hovered_, index) != index;
}

std::optional<std::size_t> Power::slot_at(int x, int y) const {
    for (std::size_t i = 0; i < actions_.size(); ++i) {
        if (slot(i).padded(spacing_ / 2).contains(x, y)) {
            return i;
        }
    }
    return std::nullopt;
}

} // namespace modules
