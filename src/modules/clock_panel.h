#pragma once

#include "app/module.h"

namespace modules {

class ClockPanel : public app::Module {
  public:
    ClockPanel() = default;

    void layout(int width, int height) override;
    render::Rect bounds() const override;
    void draw(cairo_t *cr, const app::GreeterState &state) override;
    bool visible(const app::GreeterState &state) const override;

  private:
    render::Rect rect_;
    double scale_ = 1.0;
};

} // namespace modules
