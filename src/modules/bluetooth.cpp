#include "modules/bluetooth.h"

#include <algorithm>
#include <utility>

#include "config/bluetooth_config.h"
#include "config/style_config.h"

#include "render/icons.h"
#include "render/text.h"
#include "render/widgets.h"

namespace modules {

namespace {

bool any_connected(const service::BluetoothSnapshot &bluetooth) {
    return std::ranges::any_of(bluetooth.entries, &service::BluetoothEntry::connected);
}

bool any_connecting(const service::BluetoothSnapshot &bluetooth) {
    return std::ranges::any_of(bluetooth.entries, &service::BluetoothEntry::connecting);
}

const char *icon_for(const service::BluetoothSnapshot &bluetooth) {
    if (!bluetooth.powered) {
        return render::icon::bluetooth_off;
    }
    return any_connected(bluetooth) ? render::icon::bluetooth_connected : render::icon::bluetooth_on;
}

config::Color color_for(const service::BluetoothSnapshot &bluetooth) {
    if (any_connecting(bluetooth)) {
        return config::style::lavender;
    }
    return bluetooth.powered ? config::style::text : config::style::text_muted;
}

} // namespace

Bluetooth::Bluetooth() : panel_(content_) {}

void Bluetooth::layout(int width, int height) {
    auto scale = render::ui_scale(width, height);
    rect_ = render::place(config::bluetooth::geometry, width, height, scale);
    style_ = render::make_style(scale);
    panel_.layout(style_, rect_, width, height);
}

render::Rect Bluetooth::bounds() const {
    if (!panel_.visible()) {
        return rect_;
    }
    return rect_.united(panel_.extent().bounding_rect());
}

bool Bluetooth::visible(const app::GreeterState &state) const {
    return state.view == app::View::Clock && state.bluetooth.available;
}

void Bluetooth::draw(cairo_t *cr, const app::GreeterState &state) {
    update(state);
    if (!visible(state)) {
        panel_.close();
        return;
    }
    if (!state.bluetooth_menu_open && panel_.visible()) {
        panel_.close();
    }
    render::widgets::icon_button(cr, render::to_box(rect_), icon_for(state.bluetooth), color_for(state.bluetooth), style_, hovered_, panel_.visible());
    panel_.paint(cr);
}

bool Bluetooth::click(int x, int y, app::GreeterState &state) {
    if (!visible(state)) {
        return false;
    }
    if (rect_.contains(x, y)) {
        state.bluetooth_menu_open = !state.bluetooth_menu_open;
        if (state.bluetooth_menu_open) {
            panel_.open();
        } else {
            panel_.close();
        }
        return true;
    }
    if (!state.bluetooth_menu_open) {
        return false;
    }
    if (auto region = panel_.press(x, y)) {
        if (region->id == render::region_close) {
            state.bluetooth_menu_open = false;
            panel_.close();
        } else {
            content_.activate(*region, state);
        }
        return true;
    }
    return panel_.contains(x, y);
}

bool Bluetooth::hover(int x, int y, const app::GreeterState &state) {
    bool inside = visible(state) && rect_.contains(x, y);
    return std::exchange(hovered_, inside) != inside;
}

bool Bluetooth::scroll(double notches, const app::GreeterState &state) {
    if (!visible(state) || !state.bluetooth_menu_open) {
        return false;
    }
    return panel_.wheel(notches * style_.wheel_pixels);
}

bool Bluetooth::update(const app::GreeterState &state) {
    if (content_.snapshot() == state.bluetooth) {
        return false;
    }
    content_.set(state.bluetooth);
    panel_.invalidate();
    return true;
}

void Bluetooth::clicked_outside(app::GreeterState &state) {
    if (state.bluetooth_menu_open) {
        state.bluetooth_menu_open = false;
        panel_.close();
    }
}

} // namespace modules
