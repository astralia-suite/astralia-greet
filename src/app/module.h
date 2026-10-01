#pragma once

#include <cairo.h>
#include <chrono>
#include <optional>

#include "app/greeter_state.h"

#include "render/layout.h"

namespace app {

class Module {
  public:
    virtual ~Module() = default;

    virtual void layout(int width, int height) = 0;
    virtual render::Rect bounds() const = 0;
    virtual void draw(cairo_t *cr, const GreeterState &state) = 0;

    virtual bool visible(const GreeterState &) const {
        return true;
    }

    virtual bool click(int, int, GreeterState &) {
        return false;
    }

    virtual bool hover(int, int, const GreeterState &) {
        return false;
    }

    virtual std::optional<std::chrono::milliseconds> next_tick() const {
        return std::nullopt;
    }

    virtual bool is_backdrop() const {
        return false;
    }
};

} // namespace app
