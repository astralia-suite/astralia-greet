#include "service/input_service.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <libinput.h>
#include <libudev.h>
#include <linux/input-event-codes.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>

#include "core/log.h"

namespace service {

namespace {

int open_restricted(const char *path, int flags, void *data) {
    return static_cast<InputService *>(data)->open_device(path, flags);
}

void close_restricted(int fd, void *data) {
    static_cast<InputService *>(data)->close_device(fd);
}

constexpr libinput_interface interface{open_restricted, close_restricted};

const char *optional_string(const std::string &value) {
    return value.empty() ? nullptr : value.c_str();
}

} // namespace

InputService::InputService(core::EventLoop &loop) : loop_(loop) {}

InputService::~InputService() {
    close();
    if (state_) {
        xkb_state_unref(state_);
    }
    if (keymap_) {
        xkb_keymap_unref(keymap_);
    }
    if (xkb_) {
        xkb_context_unref(xkb_);
    }
}

bool InputService::open(const config::GeneralSettings &settings) {
    close_pending_ = false;
    if (libinput_) {
        return true;
    }
    if (!xkb_) {
        xkb_ = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    }
    if (!xkb_) {
        return false;
    }
    if (!keymap_) {
        xkb_rule_names names{};
        names.model = optional_string(settings.keyboard_model);
        names.layout = optional_string(settings.keyboard_layout);
        names.variant = optional_string(settings.keyboard_variant);
        names.options = optional_string(settings.keyboard_options);
        keymap_ = xkb_keymap_new_from_names(xkb_, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
        if (!keymap_) {
            core::warn("invalid keyboard settings, falling back to the default keymap");
            names = {};
            keymap_ = xkb_keymap_new_from_names(xkb_, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
        }
    }
    if (!keymap_) {
        core::error("cannot compile a keymap");
        return false;
    }
    numlock_ = settings.numlock;
    repeat_delay_ = std::max(settings.repeat_delay, 0);
    repeat_interval_ = settings.repeat_rate > 0 ? 1000 / settings.repeat_rate : 0;
    reset_state();
    udev_ = udev_new();
    libinput_ = udev_ ? libinput_udev_create_context(&interface, this, udev_) : nullptr;
    if (!libinput_ || libinput_udev_assign_seat(libinput_, config::seat) < 0) {
        core::error("cannot initialise libinput");
        return false;
    }
    loop_.add_fd(libinput_get_fd(libinput_), EPOLLIN, [this](std::uint32_t) { dispatch(); });
    dispatch();
    return true;
}

void InputService::use_seat(SeatService *seat) {
    seat_ = seat;
}

int InputService::open_device(const char *path, int flags) {
    if (seat_ && seat_->is_open()) {
        int fd = seat_->open_device(path);
        return fd < 0 ? -EACCES : fd;
    }
    int fd = ::open(path, flags | O_CLOEXEC);
    return fd < 0 ? -errno : fd;
}

void InputService::close_device(int fd) {
    if (seat_ && seat_->is_open()) {
        seat_->close_device(fd);
        return;
    }
    ::close(fd);
}

void InputService::close() {
    stop_repeat();
    if (dispatching_) {
        close_pending_ = true;
        return;
    }
    close_pending_ = false;
    if (libinput_) {
        loop_.remove_fd(libinput_get_fd(libinput_));
        libinput_unref(libinput_);
        libinput_ = nullptr;
    }
    if (udev_) {
        udev_unref(udev_);
        udev_ = nullptr;
    }
    suspended_ = false;
}

void InputService::set_bounds(int width, int height) {
    width_ = width;
    height_ = height;
    pointer_x_ = width / 2.0;
    pointer_y_ = height / 2.0;
}

void InputService::suspend() {
    stop_repeat();
    if (libinput_ && !suspended_) {
        libinput_suspend(libinput_);
        suspended_ = true;
    }
}

void InputService::resume() {
    if (libinput_ && suspended_) {
        reset_state();
        libinput_resume(libinput_);
        suspended_ = false;
        dispatch();
    }
}

bool InputService::caps_lock() const {
    return state_ && xkb_state_mod_name_is_active(state_, XKB_MOD_NAME_CAPS, XKB_STATE_MODS_LOCKED) > 0;
}

std::string InputService::layout_name() const {
    if (!state_) {
        return {};
    }
    const char *name = xkb_keymap_layout_get_name(keymap_, xkb_state_serialize_layout(state_, XKB_STATE_LAYOUT_EFFECTIVE));
    return name ? name : "";
}

void InputService::dispatch() {
    dispatching_ = true;
    libinput_dispatch(libinput_);
    while (!close_pending_) {
        libinput_event *event = libinput_get_event(libinput_);
        if (!event) {
            break;
        }
        handle(event);
        libinput_event_destroy(event);
    }
    dispatching_ = false;
    if (close_pending_) {
        close();
    }
}

void InputService::handle(libinput_event *event) {
    switch (libinput_event_get_type(event)) {
    case LIBINPUT_EVENT_DEVICE_ADDED: {
        libinput_device *device = libinput_event_get_device(event);
        if (libinput_device_config_tap_get_finger_count(device) > 0) {
            libinput_device_config_tap_set_enabled(device, LIBINPUT_CONFIG_TAP_ENABLED);
        }
        break;
    }
    case LIBINPUT_EVENT_KEYBOARD_KEY: {
        libinput_event_keyboard *key = libinput_event_get_keyboard_event(event);
        std::uint32_t code = libinput_event_keyboard_get_key(key) + 8;
        bool pressed = libinput_event_keyboard_get_key_state(key) == LIBINPUT_KEY_STATE_PRESSED;
        xkb_state_update_key(state_, code, pressed ? XKB_KEY_DOWN : XKB_KEY_UP);
        if (pressed) {
            emit(code);
            if (xkb_keymap_key_repeats(keymap_, code)) {
                start_repeat(code);
            }
        } else if (code == repeat_key_) {
            stop_repeat();
        }
        break;
    }
    case LIBINPUT_EVENT_POINTER_MOTION: {
        libinput_event_pointer *pointer = libinput_event_get_pointer_event(event);
        move_pointer(pointer_x_ + libinput_event_pointer_get_dx(pointer), pointer_y_ + libinput_event_pointer_get_dy(pointer));
        break;
    }
    case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE: {
        libinput_event_pointer *pointer = libinput_event_get_pointer_event(event);
        move_pointer(libinput_event_pointer_get_absolute_x_transformed(pointer, static_cast<std::uint32_t>(width_)), libinput_event_pointer_get_absolute_y_transformed(pointer, static_cast<std::uint32_t>(height_)));
        break;
    }
    case LIBINPUT_EVENT_POINTER_BUTTON: {
        libinput_event_pointer *pointer = libinput_event_get_pointer_event(event);
        if (libinput_event_pointer_get_button(pointer) == BTN_LEFT && libinput_event_pointer_get_button_state(pointer) == LIBINPUT_BUTTON_STATE_PRESSED && on_click) {
            on_click(static_cast<int>(pointer_x_), static_cast<int>(pointer_y_));
        }
        break;
    }
    case LIBINPUT_EVENT_TOUCH_DOWN: {
        libinput_event_touch *touch = libinput_event_get_touch_event(event);
        move_pointer(libinput_event_touch_get_x_transformed(touch, static_cast<std::uint32_t>(width_)), libinput_event_touch_get_y_transformed(touch, static_cast<std::uint32_t>(height_)));
        if (on_click) {
            on_click(static_cast<int>(pointer_x_), static_cast<int>(pointer_y_));
        }
        break;
    }
    default:
        break;
    }
}

void InputService::emit(std::uint32_t keycode) {
    if (!on_key) {
        return;
    }
    KeyEvent key;
    key.keysym = xkb_state_key_get_one_sym(state_, keycode);
    std::array<char, 64> buffer{};
    int length = xkb_state_key_get_utf8(state_, keycode, buffer.data(), buffer.size());
    if (length > 0) {
        key.text.assign(buffer.data(), static_cast<std::size_t>(std::min(length, static_cast<int>(buffer.size()) - 1)));
    }
    key.ctrl = xkb_state_mod_name_is_active(state_, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0;
    key.alt = xkb_state_mod_name_is_active(state_, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0;
    key.shift = xkb_state_mod_name_is_active(state_, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) > 0;
    on_key(key);
}

void InputService::start_repeat(std::uint32_t keycode) {
    stop_repeat();
    if (repeat_interval_ <= 0) {
        return;
    }
    repeat_key_ = keycode;
    repeat_timer_ = loop_.add_timer(std::chrono::milliseconds(repeat_delay_), std::chrono::milliseconds(repeat_interval_), [this] { emit(repeat_key_); });
}

void InputService::stop_repeat() {
    if (repeat_timer_ >= 0) {
        loop_.remove_timer(repeat_timer_);
    }
    repeat_timer_ = -1;
    repeat_key_ = 0;
}

void InputService::reset_state() {
    if (state_) {
        xkb_state_unref(state_);
    }
    state_ = xkb_state_new(keymap_);
    if (numlock_) {
        auto index = xkb_keymap_mod_get_index(keymap_, XKB_MOD_NAME_NUM);
        if (index != XKB_MOD_INVALID) {
            xkb_state_update_mask(state_, 0, 0, 1u << index, 0, 0, 0);
        }
    }
}

void InputService::move_pointer(double x, double y) {
    pointer_x_ = std::clamp(x, 0.0, std::max(0.0, width_ - 1.0));
    pointer_y_ = std::clamp(y, 0.0, std::max(0.0, height_ - 1.0));
    if (on_pointer_motion) {
        on_pointer_motion(static_cast<int>(pointer_x_), static_cast<int>(pointer_y_));
    }
}

} // namespace service
