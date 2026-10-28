#pragma once

#include <string_view>
#include <vector>

#include "app/greeter_state.h"

#include "render/panel_content.h"
#include "render/widgets.h"

namespace modules {

class BluetoothContent : public render::PanelContent {
  public:
    static constexpr int connect_region = 1;

    void set(const service::BluetoothSnapshot &snapshot);
    const service::BluetoothSnapshot &snapshot() const;
    void activate(const render::PanelRegion &region, app::GreeterState &state) const;

    std::string_view title() const override;
    bool has_toggle() const override;
    bool toggle_on() const override;
    double content_height(const render::Style &style) const override;
    void paint(cairo_t *cr, const render::Box &view, const render::Style &style, render::PanelPaint &paint) const override;

  private:
    std::vector<render::widgets::ListRow> rows() const;

    service::BluetoothSnapshot snapshot_;
};

} // namespace modules
