#pragma once

#include <string>

#include "app/module.h"

#include "config/theme_config.h"

#include "render/canvas.h"
#include "render/style.h"

namespace modules {

class Password : public app::Module {
  public:
    Password();

    void layout(int width, int height) override;
    render::Rect bounds() const override;
    void draw(cairo_t *cr, const app::GreeterState &state) override;
    bool visible(const app::GreeterState &state) const override;
    bool click(int x, int y, app::GreeterState &state) override;
    bool hover(int x, int y, const app::GreeterState &state) override;

  private:
    config::WidgetGeometry geometry_;
    render::SurfacePtr echo_image_;
    render::SurfacePtr echo_;
    render::Style style_;
    double scale_ = 1.0;
    int echo_size_ = 0;
    render::Rect rect_;
    render::Rect button_;
    std::string font_;
    std::string icon_font_;
    std::string mask_;
    std::string placeholder_;
    std::string busy_text_;
    bool hovered_ = false;
};

} // namespace modules
