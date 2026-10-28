#include "modules/network/content.h"

#include "config/network_config.h"
#include "config/style_config.h"

#include "core/strings.h"

#include "render/icons.h"

namespace modules {

namespace {

using render::widgets::ListRow;

ListRow label(const char *text) {
    ListRow row;
    row.kind = ListRow::Kind::Section;
    row.text = text;
    return row;
}

ListRow message(const char *text, ListRow::Kind kind = ListRow::Kind::Message) {
    ListRow row;
    row.kind = kind;
    row.text = text;
    return row;
}

const char *signal_glyph(int band) {
    switch (band) {
    case 3:
        return render::icon::wifi;
    case 2:
        return render::icon::wifi2;
    case 1:
        return render::icon::wifi1;
    default:
        return render::icon::wifi0;
    }
}

ListRow entry_row(const service::NetworkEntry &entry, int index) {
    namespace colors = config::style;
    bool supported = service::security_supported(entry.security);
    ListRow row;
    row.kind = ListRow::Kind::Device;
    row.index = index;
    row.device.glyph = signal_glyph(entry.band);
    row.device.title = entry.name;
    if (entry.connecting) {
        row.device.subtitle = "";
    } else if (entry.active) {
        row.device.subtitle = config::network::connected_subtitle;
    } else if (entry.saved) {
        row.device.subtitle = config::network::saved_subtitle;
    } else {
        row.device.subtitle = supported ? service::security_label(entry.security) : config::network::unsupported_subtitle;
    }
    row.device.busy_text = config::network::busy_text;
    row.device.busy = entry.connecting;
    row.device.background = entry.active ? colors::accent_alpha25 : entry.connecting ? colors::accent_alpha12
                                                                                     : colors::text_alpha06;
    row.device.glyph_color = entry.active ? colors::accent : colors::text_dim;
    row.device.title_color = entry.active ? colors::accent : colors::text;
    row.device.action_glyph = entry.active ? render::icon::network_disconnect : (entry.saved || supported) ? render::icon::network_connect
                                                                                                           : nullptr;
    row.device.action_color = entry.active ? colors::accent : colors::text;
    return row;
}

} // namespace

void NetworkContent::set(const service::NetworkSnapshot &snapshot) {
    snapshot_ = snapshot;
}

bool NetworkContent::set_dialog(const app::GreeterState &state) {
    Dialog next{state.wifi_dialog_open(), state.wifi_ssid, core::utf8_length(state.wifi_password), core::utf8_length(state.wifi_password) >= state.wifi_min_length};
    if (next == dialog_) {
        return false;
    }
    dialog_ = std::move(next);
    return true;
}

const service::NetworkSnapshot &NetworkContent::snapshot() const {
    return snapshot_;
}

void NetworkContent::activate(const render::PanelRegion &region, app::GreeterState &state) const {
    switch (region.id) {
    case render::region_toggle:
        state.request = app::Request::ToggleWifi;
        return;
    case render::region_action:
        state.request = app::Request::ScanNetworks;
        return;
    case dialog_close_region:
    case dialog_cancel_region:
        state.clear_wifi_dialog();
        return;
    case dialog_connect_region:
        if (state.wifi_dialog_open() && core::utf8_length(state.wifi_password) >= state.wifi_min_length) {
            state.request = app::Request::ConnectNetwork;
            state.request_target = state.wifi_ap;
        }
        return;
    case connect_region:
        break;
    default:
        return;
    }
    if (region.index < 0 || static_cast<std::size_t>(region.index) >= snapshot_.entries.size()) {
        return;
    }
    const auto &entry = snapshot_.entries[static_cast<std::size_t>(region.index)];
    if (entry.saved) {
        state.request = app::Request::ActivateNetwork;
        state.request_target = entry.path;
        return;
    }
    if (!service::security_supported(entry.security)) {
        return;
    }
    state.clear_wifi_dialog();
    if (entry.security == service::NetworkSecurity::Open) {
        state.request = app::Request::ConnectNetwork;
        state.request_target = entry.ap;
        return;
    }
    state.wifi_ap = entry.ap;
    state.wifi_ssid = entry.name;
    state.wifi_min_length = entry.security == service::NetworkSecurity::Psk ? config::network::psk_min_length : 1;
}

std::string_view NetworkContent::title() const {
    return config::network::title;
}

bool NetworkContent::has_toggle() const {
    return snapshot_.wifi_present;
}

bool NetworkContent::toggle_on() const {
    return snapshot_.wifi_enabled;
}

const char *NetworkContent::header_action() const {
    return snapshot_.wifi_present && snapshot_.wifi_enabled ? render::icon::refresh : nullptr;
}

double NetworkContent::dialog_reserve(const render::Style &style) const {
    return snapshot_.wifi_present ? style.gap_below_icon + render::widgets::password_dialog_height(style) : 0.0;
}

double NetworkContent::dialog_height(const render::Style &style) const {
    return dialog_.open ? render::widgets::password_dialog_height(style) : 0.0;
}

void NetworkContent::paint_dialog(cairo_t *cr, const render::Box &box, const render::Style &style, render::PanelPaint &paint) const {
    render::widgets::PasswordDialog dialog;
    dialog.ssid = dialog_.ssid;
    dialog.placeholder = config::network::password_placeholder;
    dialog.connect_text = config::network::connect_text;
    dialog.cancel_text = config::network::cancel_text;
    dialog.length = dialog_.length;
    dialog.can_connect = dialog_.can_connect;
    render::widgets::password_dialog(cr, box, dialog, style, paint, dialog_close_region, dialog_connect_region, dialog_cancel_region);
}

std::vector<ListRow> NetworkContent::rows() const {
    std::vector<ListRow> rows;
    if (!snapshot_.error.empty()) {
        rows.push_back(message(snapshot_.error.c_str(), ListRow::Kind::Error));
    }
    if (snapshot_.link == service::NetworkLink::Wired) {
        rows.push_back(message(config::network::wired_text, ListRow::Kind::Banner));
    }
    if (!snapshot_.wifi_present) {
        if (rows.empty()) {
            rows.push_back(message(config::network::no_adapter_text));
        }
        return rows;
    }
    if (!snapshot_.wifi_enabled) {
        rows.push_back(message(config::network::disabled_text));
        return rows;
    }
    if (snapshot_.entries.empty()) {
        rows.push_back(message(config::network::empty_text));
        return rows;
    }
    struct Group {
        const char *label;
        bool (*accepts)(const service::NetworkEntry &);
    };
    constexpr Group groups[]{
        {config::network::connected_label, [](const service::NetworkEntry &entry) { return entry.active || entry.connecting; }},
        {config::network::known_label, [](const service::NetworkEntry &entry) { return entry.saved && !entry.active && !entry.connecting; }},
        {config::network::available_label, [](const service::NetworkEntry &entry) { return !entry.saved; }},
    };
    for (const auto &group : groups) {
        bool labelled = false;
        for (std::size_t i = 0; i < snapshot_.entries.size(); ++i) {
            if (!group.accepts(snapshot_.entries[i])) {
                continue;
            }
            if (!labelled) {
                rows.push_back(label(group.label));
                labelled = true;
            }
            rows.push_back(entry_row(snapshot_.entries[i], static_cast<int>(i)));
        }
    }
    return rows;
}

double NetworkContent::content_height(const render::Style &style) const {
    return render::widgets::list_height(rows(), style);
}

void NetworkContent::paint(cairo_t *cr, const render::Box &view, const render::Style &style, render::PanelPaint &paint) const {
    render::widgets::paint_list(cr, view, rows(), style, paint, connect_region);
}

} // namespace modules
