#include "modules/bluetooth/content.h"

#include "config/bluetooth_config.h"
#include "config/style_config.h"

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

ListRow message(const char *text) {
    ListRow row;
    row.kind = ListRow::Kind::Message;
    row.text = text;
    return row;
}

ListRow entry_row(const service::BluetoothEntry &entry, int index) {
    namespace defaults = config::style;
    ListRow row;
    row.kind = ListRow::Kind::Device;
    row.index = index;
    row.device.glyph = render::icon::bluetooth_on;
    row.device.title = entry.name;
    row.device.subtitle = entry.connecting ? "" : entry.connected ? config::bluetooth::connected_subtitle
                                                                  : config::bluetooth::paired_subtitle;
    row.device.busy_text = config::bluetooth::busy_text;
    row.device.busy = entry.connecting;
    row.device.background = entry.connected ? defaults::accent_alpha25 : entry.connecting ? defaults::accent_alpha12
                                                                                          : defaults::text_alpha06;
    row.device.glyph_color = entry.connected ? defaults::accent : defaults::text_dim;
    row.device.title_color = entry.connected ? defaults::accent : defaults::text;
    row.device.action_glyph = entry.connected ? render::icon::network_disconnect : render::icon::network_connect;
    row.device.action_color = entry.connected ? defaults::accent : defaults::text;
    return row;
}

} // namespace

void BluetoothContent::set(const service::BluetoothSnapshot &snapshot) {
    snapshot_ = snapshot;
}

const service::BluetoothSnapshot &BluetoothContent::snapshot() const {
    return snapshot_;
}

void BluetoothContent::activate(const render::PanelRegion &region, app::GreeterState &state) const {
    if (region.id == render::region_toggle) {
        state.request = app::Request::ToggleBluetooth;
    } else if (region.id == connect_region && region.index >= 0 && static_cast<std::size_t>(region.index) < snapshot_.entries.size()) {
        state.request = app::Request::ToggleBluetoothDevice;
        state.request_target = snapshot_.entries[static_cast<std::size_t>(region.index)].path;
    }
}

std::string_view BluetoothContent::title() const {
    return config::bluetooth::title;
}

bool BluetoothContent::has_toggle() const {
    return snapshot_.available;
}

bool BluetoothContent::toggle_on() const {
    return snapshot_.powered;
}

std::vector<ListRow> BluetoothContent::rows() const {
    std::vector<ListRow> rows;
    if (!snapshot_.powered) {
        rows.push_back(message(config::bluetooth::off_text));
        return rows;
    }
    if (snapshot_.entries.empty()) {
        rows.push_back(message(config::bluetooth::empty_text));
        return rows;
    }
    for (bool connected : {true, false}) {
        bool labelled = false;
        for (std::size_t i = 0; i < snapshot_.entries.size(); ++i) {
            const auto &entry = snapshot_.entries[i];
            if ((entry.connected || entry.connecting) != connected) {
                continue;
            }
            if (!labelled) {
                rows.push_back(label(connected ? config::bluetooth::connected_label : config::bluetooth::paired_label));
                labelled = true;
            }
            rows.push_back(entry_row(entry, static_cast<int>(i)));
        }
    }
    return rows;
}

double BluetoothContent::content_height(const render::Style &style) const {
    return render::widgets::list_height(rows(), style);
}

void BluetoothContent::paint(cairo_t *cr, const render::Box &view, const render::Style &style, render::PanelPaint &paint) const {
    render::widgets::paint_list(cr, view, rows(), style, paint, connect_region);
}

} // namespace modules
