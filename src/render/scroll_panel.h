#pragma once

#include <cairo.h>
#include <optional>
#include <vector>

#include "render/canvas.h"
#include "render/layout.h"
#include "render/panel_content.h"
#include "render/style.h"

namespace render {

class ScrollPanel {
  public:
    explicit ScrollPanel(const PanelContent &content);

    void layout(const Style &style, const Rect &icon, int screen_width, int screen_height);
    void open();
    void close();
    void invalidate();
    void paint(cairo_t *cr);
    std::optional<PanelRegion> press(double x, double y) const;
    bool wheel(double pixels);
    bool visible() const;
    bool contains(double x, double y) const;
    Box extent() const;
    double scroll_offset() const;

  private:
    void rebuild();
    void place();
    void paint_chrome(cairo_t *cr);
    void build_shell();
    void paint_body_to(double bottom);
    Box dialog_area() const;
    double max_scroll() const;

    const PanelContent &content_;
    Style style_;
    Rect icon_;
    int screen_width_ = 0;
    int screen_height_ = 0;
    Box card_;
    double view_top_ = 0.0;
    double view_height_ = 0.0;
    double content_height_ = 0.0;
    double scroll_ = 0.0;
    double painted_to_ = 0.0;
    bool open_ = false;
    SurfacePtr chrome_;
    SurfacePtr body_;
    SurfacePtr shell_;
    Box shell_origin_;
    std::vector<PanelRegion> header_regions_;
    std::vector<PanelRegion> body_regions_;
    std::vector<PanelRegion> dialog_regions_;
};

} // namespace render
