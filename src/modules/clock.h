#pragma once

#include <string>

#include "app/module.h"

#include "config/theme_config.h"

#include "render/text.h"

namespace modules {

class Clock : public app::Module {
  public:
    Clock();

    void layout(int width, int height) override;
    render::Rect bounds() const override;
    void draw(cairo_t *cr, const app::GreeterState &state) override;
    bool visible(const app::GreeterState &state) const override;
    bool click(int x, int y, app::GreeterState &state) override;
    std::optional<std::chrono::milliseconds> next_tick() const override;

  private:
    config::WidgetGeometry geometry_;
    render::Rect rect_;
    std::string hour_format_;
    std::string minute_format_;
    std::string font_;
    config::Color hour_color_;
    config::Color minute_color_;
    render::TextAlign align_;
    bool seconds_ = false;
};

} // namespace modules
