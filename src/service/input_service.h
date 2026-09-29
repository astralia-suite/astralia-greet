#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "config/greet_config.h"

#include "core/event_loop.h"

#include "service/seat_service.h"

struct libinput;
struct libinput_event;
struct udev;
struct xkb_context;
struct xkb_keymap;
struct xkb_state;

namespace service {

struct KeyEvent {
    std::uint32_t keysym = 0;
    std::string text;
    bool ctrl = false;
    bool alt = false;
    bool shift = false;
};

class InputService {
  public:
    explicit InputService(core::EventLoop &loop);
    ~InputService();
    InputService(const InputService &) = delete;
    InputService &operator=(const InputService &) = delete;

    void use_seat(SeatService *seat);
    bool open(const config::GeneralSettings &settings);
    void close();
    int open_device(const char *path, int flags);
    void close_device(int fd);
    void set_bounds(int width, int height);
    void suspend();
    void resume();
    bool caps_lock() const;
    std::string layout_name() const;

    std::function<void(const KeyEvent &)> on_key;
    std::function<void(int, int)> on_pointer_motion;
    std::function<void(int, int)> on_click;

  private:
    void dispatch();
    void handle(libinput_event *event);
    void emit(std::uint32_t keycode);
    void start_repeat(std::uint32_t keycode);
    void stop_repeat();
    void reset_state();
    void move_pointer(double x, double y);

    core::EventLoop &loop_;
    SeatService *seat_ = nullptr;
    udev *udev_ = nullptr;
    libinput *libinput_ = nullptr;
    xkb_context *xkb_ = nullptr;
    xkb_keymap *keymap_ = nullptr;
    xkb_state *state_ = nullptr;
    bool numlock_ = false;
    bool suspended_ = false;
    bool dispatching_ = false;
    bool close_pending_ = false;
    int repeat_delay_ = 400;
    int repeat_interval_ = 33;
    int repeat_timer_ = -1;
    std::uint32_t repeat_key_ = 0;
    int width_ = 0;
    int height_ = 0;
    double pointer_x_ = 0;
    double pointer_y_ = 0;
};

} // namespace service
