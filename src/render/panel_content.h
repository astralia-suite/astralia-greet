#pragma once

#include <cairo.h>
#include <string_view>
#include <vector>

#include "config/style_config.h"
#include "config/theme_config.h"

#include "render/layout.h"
#include "render/style.h"

namespace render {

inline constexpr int region_close = -1;
inline constexpr int region_toggle = -2;
inline constexpr int region_action = -3;

struct PanelRegion {
    Box box;
    int id = 0;
    int index = 0;
};

class PanelPaint {
  public:
    explicit PanelPaint(std::vector<PanelRegion> &regions) : regions_(regions) {}

    void region(const Box &box, int id, int index = 0) {
        regions_.push_back({box, id, index});
    }

  private:
    std::vector<PanelRegion> &regions_;
};

class PanelContent {
  public:
    virtual ~PanelContent() = default;

    virtual std::string_view title() const = 0;
    virtual const config::Color &tint() const {
        return config::style::accent;
    }
    virtual bool has_toggle() const {
        return false;
    }
    virtual bool toggle_on() const {
        return false;
    }
    virtual const char *header_action() const {
        return nullptr;
    }
    virtual double dialog_reserve(const Style &) const {
        return 0.0;
    }
    virtual double dialog_height(const Style &) const {
        return 0.0;
    }
    virtual void paint_dialog(cairo_t *, const Box &, const Style &, PanelPaint &) const {}
    virtual double content_height(const Style &style) const = 0;
    // paints the rows whose top lies in [view.y, view.y + view.h) at their absolute y, so a body can be painted in bands
    virtual void paint(cairo_t *cr, const Box &view, const Style &style, PanelPaint &paint) const = 0;
};

} // namespace render
