#pragma once

#include <cairo.h>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <xcb/xcb.h>

#include "core/event_loop.h"

#include "render/canvas.h"
#include "render/layout.h"

#include "service/input_service.h"

struct xkb_context;
struct xkb_keymap;
struct xkb_state;

namespace service {

class WindowService {
  public:
    explicit WindowService(core::EventLoop &loop);
    ~WindowService();
    WindowService(const WindowService &) = delete;
    WindowService &operator=(const WindowService &) = delete;

    bool open();
    int width() const;
    int height() const;
    void present(const render::Canvas &canvas, const std::vector<render::Rect> &damage);
    bool caps_lock() const;
    std::string layout_name() const;
    void close();

    std::function<void(const KeyEvent &)> on_key;
    std::function<void(int, int)> on_click;
    std::function<void(int, int)> on_pointer_motion;
    std::function<void(int, int)> on_resize;
    std::function<void(const render::Rect &)> on_expose;
    std::function<void()> on_modifiers;
    std::function<void()> on_close;

  private:
    bool setup_keyboard();
    bool reload_state();
    void dispatch();
    void handle(xcb_generic_event_t *event);
    void emit(std::uint32_t keycode);

    core::EventLoop &loop_;
    xcb_connection_t *connection_ = nullptr;
    xcb_window_t window_ = 0;
    xcb_atom_t delete_atom_ = 0;
    std::int32_t device_ = -1;
    cairo_surface_t *surface_ = nullptr;
    xkb_context *xkb_ = nullptr;
    xkb_keymap *keymap_ = nullptr;
    xkb_state *state_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

} // namespace service
