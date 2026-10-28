#include "render/layout.h"
#include "render/panel_content.h"
#include "render/scroll.h"
#include "render/scroll_panel.h"
#include "render/style.h"

#include "test/check.h"

namespace {

class FixedContent : public render::PanelContent {
  public:
    explicit FixedContent(double height) : height_(height) {}

    std::string_view title() const override {
        return "Test";
    }

    double content_height(const render::Style &) const override {
        return height_;
    }

    void paint(cairo_t *, const render::Box &, const render::Style &, render::PanelPaint &paint) const override {
        paint.region({10.0, 20.0, 30.0, 40.0}, 7, 3);
    }

  private:
    double height_;
};

class DialogContent : public FixedContent {
  public:
    DialogContent() : FixedContent(300.0) {}

    double dialog_reserve(const render::Style &) const override {
        return 130.0;
    }

    double dialog_height(const render::Style &) const override {
        return open ? 100.0 : 0.0;
    }

    void paint_dialog(cairo_t *, const render::Box &, const render::Style &, render::PanelPaint &paint) const override {
        paint.region({10.0, 10.0, 20.0, 20.0}, 9, 0);
    }

    bool open = false;
};

void test_dialog() {
    auto style = render::make_style(1.0);
    DialogContent content;
    render::ScrollPanel panel(content);
    panel.layout(style, {1000, 20, 28, 28}, 1280, 800);
    panel.open();
    auto closed = panel.extent();
    content.open = true;
    auto open = panel.extent();
    CHECK(open.y == closed.y);
    CHECK(open.y + open.h > closed.y + closed.h + 90.0);
    CHECK(open.y + open.h <= 800.0);
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1280, 800);
    cairo_t *cr = cairo_create(surface);
    panel.paint(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    double dialog_top = closed.y + closed.h + style.gap_below_icon;
    CHECK(panel.contains(open.x + style.overhang + 5.0, open.y + open.h - 5.0));
    CHECK(!panel.contains(open.x + style.overhang + 5.0, open.y + open.h + 5.0));
    auto region = panel.press(open.x + style.overhang + 15.0, dialog_top + 15.0);
    CHECK(region && region->id == 9);
    CHECK(!panel.press(open.x + style.overhang + 100.0, dialog_top + 60.0));
    content.open = false;
    CHECK(panel.extent().h == closed.h);
}

void test_geometry() {
    auto style = render::make_style(1.0);
    render::Box panel{100.0, 50.0, 300.0, 200.0};
    auto geometry = render::scroll_geometry(panel, style);
    CHECK(geometry.paper.h == panel.h);
    CHECK(geometry.second_roller.y == panel.y + panel.h);
    CHECK(geometry.first_roller.y == panel.y - style.roller);
    CHECK(geometry.first_roller.x == panel.x - style.overhang);
    CHECK(geometry.first_roller.w == panel.w + 2.0 * style.overhang);
    auto bounds = render::scroll_bounds(geometry);
    CHECK(bounds.y == geometry.first_roller.y);
    CHECK(bounds.y + bounds.h == geometry.second_roller.y + geometry.second_roller.h);
}

void test_panel() {
    auto style = render::make_style(1.0);
    FixedContent content(2000.0);
    render::ScrollPanel panel(content);
    panel.layout(style, {1000, 20, 28, 28}, 1280, 800);
    CHECK(!panel.visible());
    CHECK(!panel.wheel(40.0));
    CHECK(!panel.press(1000.0, 100.0));
    panel.open();
    CHECK(panel.visible());
    auto extent = panel.extent();
    CHECK(extent.y > 0.0);
    CHECK(extent.y + extent.h <= 800.0);
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1280, 800);
    cairo_t *cr = cairo_create(surface);
    panel.paint(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    CHECK(panel.wheel(40.0));
    CHECK(panel.scroll_offset() == 40.0);
    CHECK(panel.wheel(1.0e6));
    CHECK(panel.scroll_offset() > 0.0 && panel.scroll_offset() < 2000.0);
    double bottom = panel.scroll_offset();
    CHECK(!panel.wheel(10.0));
    CHECK(panel.scroll_offset() == bottom);
    CHECK(panel.wheel(-1.0e6));
    CHECK(panel.scroll_offset() == 0.0);
    CHECK(panel.contains(extent.x + 1.0, extent.y + 1.0));
    CHECK(!panel.contains(0.0, 0.0));
    panel.close();
    CHECK(!panel.visible());
    CHECK(!panel.wheel(10.0));
    CHECK(!panel.contains(extent.x + 1.0, extent.y + 1.0));
}

} // namespace

void test_scroll() {
    test_geometry();
    test_panel();
    test_dialog();
}
