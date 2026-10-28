#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "app/greeter_state.h"

#include "render/panel_content.h"
#include "render/widgets.h"

namespace modules {

class NetworkContent : public render::PanelContent {
  public:
    static constexpr int connect_region = 1;
    static constexpr int dialog_close_region = 2;
    static constexpr int dialog_connect_region = 3;
    static constexpr int dialog_cancel_region = 4;

    void set(const service::NetworkSnapshot &snapshot);
    bool set_dialog(const app::GreeterState &state);
    const service::NetworkSnapshot &snapshot() const;
    void activate(const render::PanelRegion &region, app::GreeterState &state) const;

    std::string_view title() const override;
    bool has_toggle() const override;
    bool toggle_on() const override;
    const char *header_action() const override;
    double dialog_reserve(const render::Style &style) const override;
    double dialog_height(const render::Style &style) const override;
    void paint_dialog(cairo_t *cr, const render::Box &box, const render::Style &style, render::PanelPaint &paint) const override;
    double content_height(const render::Style &style) const override;
    void paint(cairo_t *cr, const render::Box &view, const render::Style &style, render::PanelPaint &paint) const override;

  private:
    struct Dialog {
        bool open = false;
        std::string ssid;
        std::size_t length = 0;
        bool can_connect = false;

        bool operator==(const Dialog &) const = default;
    };

    std::vector<render::widgets::ListRow> rows() const;

    service::NetworkSnapshot snapshot_;
    Dialog dialog_;
};

} // namespace modules
