#pragma once

#include <string>

#include "app/module.h"

#include "config/theme_config.h"

#include "render/text.h"

namespace modules {

class Date : public app::Module {
  public:
    Date();

    void layout(int width, int height) override;
    render::Rect bounds() const override;
    void draw(cairo_t *cr, const app::GreeterState &state) override;
    bool visible(const app::GreeterState &state) const override;
    std::optional<std::chrono::milliseconds> next_tick() const override;

  private:
    config::WidgetGeometry geometry_;
    render::Rect rect_;
    std::string format_;
    std::string font_;
    config::Color color_;
    render::TextAlign align_;
};

} // namespace modules
