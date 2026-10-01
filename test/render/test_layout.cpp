#include "render/layout.h"

#include "test/check.h"

void test_layout() {
    auto center = render::place({config::Anchor::Center, 0, 20, 300, 46}, 1280, 800, 1.0);
    CHECK(center.x == 490 && center.y == 397 && center.width == 300 && center.height == 46);
    auto corner = render::place({config::Anchor::BottomRight, -28, -28, 100, 30}, 1280, 800, 1.0);
    CHECK(corner.x == 1152 && corner.y == 742);
    auto top = render::place({config::Anchor::Top, 0, 80, 560, 150}, 1280, 800, 1.0);
    CHECK(top.x == 360 && top.y == 80);
    auto left = render::place({config::Anchor::Left, 10, 0, 100, 100}, 1280, 800, 1.0);
    CHECK(left.x == 10 && left.y == 350);

    CHECK(render::ui_scale(1920, 1200) == 1.0);
    CHECK(render::ui_scale(1920, 1080) == 0.9);
    CHECK(render::ui_scale(7680, 2160) == 1.8);
    CHECK(render::ui_scale(1600, 1200) == 1600.0 / 1920);
    auto small = render::place({config::Anchor::BottomRight, -30, -30, 300, 60}, 1280, 800, 2.0 / 3.0);
    CHECK(small.width == 200 && small.height == 40 && small.x == 1060 && small.y == 740);
    CHECK(render::line_width(1.0, 0.64) == 1.0 && render::line_width(2.0, 1.8) == 3.6);

    render::Rect a{0, 0, 10, 10};
    render::Rect b{5, 5, 10, 10};
    CHECK(a.intersects(b));
    CHECK(!a.intersects({10, 0, 5, 5}));
    auto inter = a.intersected(b);
    CHECK(inter.x == 5 && inter.y == 5 && inter.width == 5 && inter.height == 5);
    CHECK(a.contains(9, 9) && !a.contains(10, 10));
    auto padded = a.padded(2);
    CHECK(padded.x == -2 && padded.width == 14);
    auto united = a.united(b);
    CHECK(united.x == 0 && united.y == 0 && united.width == 15 && united.height == 15);
    CHECK(a.united({}).width == 10);
    auto merged = render::merge_overlapping({a, {20, 20, 5, 5}, b, {}, {14, 14, 8, 8}});
    CHECK(merged.size() == 1 && merged[0].x == 0 && merged[0].width == 25 && merged[0].height == 25);
    auto apart = render::merge_overlapping({a, {20, 20, 5, 5}});
    CHECK(apart.size() == 2);

    auto fill = render::fit_image(1920, 1080, 1280, 800, config::BackgroundMode::Fill);
    CHECK(fill.scale_x == fill.scale_y && fill.scale_x > 0.74 && fill.scale_x < 0.75);
    CHECK(fill.y == 0.0 && fill.x < 0.0);
    auto fit = render::fit_image(1920, 1080, 1280, 800, config::BackgroundMode::Fit);
    CHECK(fit.x == 0.0 && fit.y > 0.0);
    auto centered = render::fit_image(100, 50, 1280, 800, config::BackgroundMode::Center);
    CHECK(centered.scale_x == 1.0 && centered.x == 590.0 && centered.y == 375.0);
    auto stretch = render::fit_image(100, 50, 200, 200, config::BackgroundMode::Stretch);
    CHECK(stretch.scale_x == 2.0 && stretch.scale_y == 4.0);
}
