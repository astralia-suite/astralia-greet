#include "modules/network.h"

#include <utility>

#include "config/network_config.h"
#include "config/style_config.h"

#include "render/icons.h"
#include "render/text.h"
#include "render/widgets.h"

namespace modules {

namespace {

using service::NetworkLink;

const char *icon_for(const service::NetworkSnapshot &network) {
    switch (network.link) {
    case NetworkLink::Wifi:
        return render::icon::wifi;
    case NetworkLink::Wired:
        return render::icon::network_connect;
    case NetworkLink::Connecting:
        return network.wifi_present ? render::icon::wifi : render::icon::network_connect;
    case NetworkLink::None:
        break;
    }
    return network.wifi_present && !network.wifi_enabled ? render::icon::wifi_off : render::icon::network_disconnect;
}

config::Color color_for(const service::NetworkSnapshot &network) {
    switch (network.link) {
    case NetworkLink::Connecting:
        return config::style::lavender;
    case NetworkLink::None:
        return config::style::text_muted;
    default:
        return config::style::text;
    }
}

} // namespace

Network::Network() : panel_(content_) {}

void Network::layout(int width, int height) {
    auto scale = render::ui_scale(width, height);
    rect_ = render::place(config::network::geometry, width, height, scale);
    style_ = render::make_style(scale);
    panel_.layout(style_, rect_, width, height);
}

render::Rect Network::bounds() const {
    if (!panel_.visible()) {
        return rect_;
    }
    return rect_.united(panel_.extent().bounding_rect());
}

bool Network::visible(const app::GreeterState &state) const {
    return state.view == app::View::Clock && state.network.available;
}

void Network::draw(cairo_t *cr, const app::GreeterState &state) {
    update(state);
    if (!visible(state)) {
        panel_.close();
        return;
    }
    if (!state.network_menu_open && panel_.visible()) {
        panel_.close();
    }
    render::widgets::icon_button(cr, render::to_box(rect_), icon_for(state.network), color_for(state.network), style_, hovered_, panel_.visible());
    panel_.paint(cr);
}

bool Network::click(int x, int y, app::GreeterState &state) {
    if (!visible(state)) {
        return false;
    }
    if (rect_.contains(x, y)) {
        state.network_menu_open = !state.network_menu_open;
        if (state.network_menu_open) {
            panel_.open();
        } else {
            panel_.close();
        }
        return true;
    }
    if (!state.network_menu_open) {
        return false;
    }
    if (auto region = panel_.press(x, y)) {
        if (region->id == render::region_close) {
            state.network_menu_open = false;
            state.clear_wifi_dialog();
            panel_.close();
        } else {
            content_.activate(*region, state);
        }
        update(state);
        return true;
    }
    return panel_.contains(x, y);
}

bool Network::hover(int x, int y, const app::GreeterState &state) {
    bool inside = visible(state) && rect_.contains(x, y);
    return std::exchange(hovered_, inside) != inside;
}

bool Network::scroll(double notches, const app::GreeterState &state) {
    if (!visible(state) || !state.network_menu_open) {
        return false;
    }
    return panel_.wheel(notches * style_.wheel_pixels);
}

bool Network::update(const app::GreeterState &state) {
    bool changed = content_.set_dialog(state);
    if (!(content_.snapshot() == state.network)) {
        content_.set(state.network);
        panel_.invalidate();
        changed = true;
    }
    return changed;
}

void Network::clicked_outside(app::GreeterState &state) {
    if (state.network_menu_open) {
        state.network_menu_open = false;
        state.clear_wifi_dialog();
        panel_.close();
    }
}

} // namespace modules
