#pragma once

#include <string>
#include <vector>

#include "app/module.h"

#include "config/greet_config.h"
#include "config/theme_config.h"

namespace modules {

class Power : public app::Module {
  public:
    explicit Power(const config::PowerSettings &power);

    void layout(int width, int height) override;
    render::Rect bounds() const override;
    void draw(cairo_t *cr, const app::GreeterState &state) override;
    bool visible(const app::GreeterState &state) const override;
    bool click(int x, int y, app::GreeterState &state) override;

  private:
    render::Rect slot(std::size_t index) const;

    config::WidgetGeometry geometry_;
    render::Rect rect_;
    std::vector<app::Request> actions_;
    int icon_size_ = 0;
    int spacing_ = 0;
    std::string font_;
    config::Color color_;
};

} // namespace modules
