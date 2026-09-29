#pragma once

#include <string>

#include "app/module.h"

#include "config/theme_config.h"

#include "render/text.h"

namespace modules {

class Hint : public app::Module {
  public:
    Hint();

    void layout(int width, int height) override;
    render::Rect bounds() const override;
    void draw(cairo_t *cr, const app::GreeterState &state) override;
    bool visible(const app::GreeterState &state) const override;

  private:
    config::WidgetGeometry geometry_;
    render::Rect rect_;
    std::string font_;
    std::string caps_lock_text_;
    std::string idle_text_;
    bool show_layout_ = false;
    render::TextAlign align_;
    config::Color info_color_;
    config::Color warn_color_;
    config::Color error_color_;
};

} // namespace modules
