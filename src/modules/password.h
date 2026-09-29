#pragma once

#include <string>

#include "app/module.h"

#include "config/theme_config.h"

#include "render/canvas.h"

namespace modules {

class Password : public app::Module {
  public:
    Password();

    void layout(int width, int height) override;
    render::Rect bounds() const override;
    void draw(cairo_t *cr, const app::GreeterState &state) override;
    bool visible(const app::GreeterState &state) const override;
    bool click(int x, int y, app::GreeterState &state) override;

  private:
    config::WidgetGeometry geometry_;
    render::SurfacePtr echo_;
    render::Rect rect_;
    std::string font_;
    std::string mask_;
    std::string placeholder_;
    std::string busy_text_;
    config::Color text_color_;
    config::Color fill_;
    config::Color border_;
    config::Color focus_border_;
    config::Color muted_;
};

} // namespace modules
