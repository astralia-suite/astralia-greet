#pragma once

#include <string>

#include "app/module.h"

#include "modules/network/content.h"

#include "render/scroll_panel.h"
#include "render/style.h"

namespace modules {

class Network : public app::Module {
  public:
    Network();

    void layout(int width, int height) override;
    render::Rect bounds() const override;
    void draw(cairo_t *cr, const app::GreeterState &state) override;
    bool visible(const app::GreeterState &state) const override;
    bool click(int x, int y, app::GreeterState &state) override;
    bool hover(int x, int y, const app::GreeterState &state) override;
    bool scroll(double notches, const app::GreeterState &state) override;
    bool update(const app::GreeterState &state) override;
    void clicked_outside(app::GreeterState &state) override;

  private:
    NetworkContent content_;
    render::ScrollPanel panel_;
    render::Style style_;
    render::Rect rect_;
    bool hovered_ = false;
};

} // namespace modules
