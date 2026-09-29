#pragma once

#include "app/module.h"

#include "render/canvas.h"

namespace modules {

class Background : public app::Module {
  public:
    Background() = default;

    void layout(int width, int height) override;
    render::Rect bounds() const override;
    void draw(cairo_t *cr, const app::GreeterState &state) override;
    bool is_backdrop() const override;

  private:
    render::Rect rect_;
    render::SurfacePtr surface_;
};

} // namespace modules
