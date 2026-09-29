#include "modules/power.h"

#include <algorithm>

#include "config/power_config.h"

#include "render/icons.h"
#include "render/text.h"

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
    icon_size_ = defaults::icon_size;
    font_ = render::icon_font(icon_size_);
    spacing_ = defaults::spacing;
    color_ = config::muted;
    auto count = static_cast<int>(actions_.size());
    geometry_ = defaults::geometry;
    geometry_.width = count * icon_size_ + std::max(0, count - 1) * spacing_;
    geometry_.height = icon_size_;
}

void Power::layout(int width, int height) {
    rect_ = render::place(geometry_, width, height);
}

render::Rect Power::bounds() const {
    return rect_;
}

bool Power::visible(const app::GreeterState &state) const {
    return state.view == app::View::Clock;
}

render::Rect Power::slot(std::size_t index) const {
    int x = rect_.x + static_cast<int>(index) * (icon_size_ + spacing_);
    return {x, static_cast<int>(rect_.center_y()) - icon_size_ / 2, icon_size_, icon_size_};
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
            render::draw_text(cr, icon, font_, color_, slot(i), render::TextAlign::Center);
        }
    }
}

bool Power::click(int x, int y, app::GreeterState &state) {
    if (!visible(state)) {
        return false;
    }
    for (std::size_t i = 0; i < actions_.size(); ++i) {
        if (slot(i).padded(spacing_ / 2).contains(x, y)) {
            state.request = actions_[i];
            return true;
        }
    }
    return false;
}

} // namespace modules
