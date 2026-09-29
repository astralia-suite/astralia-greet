#pragma once

#include <cstddef>

#include "app/module.h"

#include "render/dropdown.h"

namespace modules {

class UserMenu : public app::Module {
  public:
    UserMenu();

    void layout(int width, int height) override;
    render::Rect bounds() const override;
    void draw(cairo_t *cr, const app::GreeterState &state) override;
    bool visible(const app::GreeterState &state) const override;
    bool click(int x, int y, app::GreeterState &state) override;

  private:
    render::Dropdown dropdown_;
    render::Rect rect_;
    int screen_height_ = 0;
    bool open_ = false;
    std::size_t user_count_ = 0;
};

} // namespace modules
