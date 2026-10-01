#include "service/window_service.h"

#include <algorithm>
#include <array>
#include <cairo-xcb.h>
#include <cstdlib>
#include <string_view>
#include <sys/epoll.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <xkbcommon/xkbcommon.h>

#include "config/window_config.h"

#include "core/log.h"

namespace service {

namespace {

xcb_screen_t *find_screen(xcb_connection_t *connection, int number) {
    for (auto screens = xcb_setup_roots_iterator(xcb_get_setup(connection)); screens.rem; --number, xcb_screen_next(&screens)) {
        if (number == 0) {
            return screens.data;
        }
    }
    return nullptr;
}

xcb_visualtype_t *find_visual(const xcb_screen_t *screen) {
    for (auto depths = xcb_screen_allowed_depths_iterator(screen); depths.rem; xcb_depth_next(&depths)) {
        for (auto visuals = xcb_depth_visuals_iterator(depths.data); visuals.rem; xcb_visualtype_next(&visuals)) {
            if (visuals.data->visual_id == screen->root_visual) {
                return visuals.data;
            }
        }
    }
    return nullptr;
}

xcb_atom_t intern(xcb_connection_t *connection, std::string_view name) {
    auto cookie = xcb_intern_atom(connection, 0, static_cast<std::uint16_t>(name.size()), name.data());
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(connection, cookie, nullptr);
    xcb_atom_t atom = reply ? reply->atom : static_cast<xcb_atom_t>(XCB_ATOM_NONE);
    std::free(reply);
    return atom;
}

} // namespace

WindowService::WindowService(core::EventLoop &loop) : loop_(loop) {}

WindowService::~WindowService() {
    close();
}

bool WindowService::open() {
    int number = 0;
    connection_ = xcb_connect(nullptr, &number);
    if (xcb_connection_has_error(connection_)) {
        core::error("cannot connect to the X server (is DISPLAY set?)");
        close();
        return false;
    }
    xcb_screen_t *screen = find_screen(connection_, number);
    xcb_visualtype_t *visual = screen ? find_visual(screen) : nullptr;
    if (!visual) {
        core::error("cannot find the X screen visual");
        close();
        return false;
    }
    if (!setup_keyboard()) {
        core::error("cannot set up the X keyboard");
        close();
        return false;
    }
    width_ = screen->width_in_pixels;
    height_ = screen->height_in_pixels;
    window_ = xcb_generate_id(connection_);
    std::uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    std::array<std::uint32_t, 2> values{screen->black_pixel, XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY};
    xcb_create_window(connection_, screen->root_depth, window_, screen->root, 0, 0, static_cast<std::uint16_t>(width_), static_cast<std::uint16_t>(height_), 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, mask, values.data());
    std::string_view title = config::window::title;
    xcb_change_property(connection_, XCB_PROP_MODE_REPLACE, window_, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, static_cast<std::uint32_t>(title.size()), title.data());
    xcb_atom_t protocols = intern(connection_, "WM_PROTOCOLS");
    delete_atom_ = intern(connection_, "WM_DELETE_WINDOW");
    xcb_change_property(connection_, XCB_PROP_MODE_REPLACE, window_, protocols, XCB_ATOM_ATOM, 32, 1, &delete_atom_);
    xcb_atom_t state = intern(connection_, "_NET_WM_STATE");
    xcb_atom_t fullscreen = intern(connection_, "_NET_WM_STATE_FULLSCREEN");
    xcb_change_property(connection_, XCB_PROP_MODE_REPLACE, window_, state, XCB_ATOM_ATOM, 32, 1, &fullscreen);
    xcb_map_window(connection_, window_);
    surface_ = cairo_xcb_surface_create(connection_, window_, visual, width_, height_);
    loop_.add_fd(xcb_get_file_descriptor(connection_), EPOLLIN, [this](std::uint32_t) { dispatch(); });
    xcb_flush(connection_);
    dispatch();
    return true;
}

bool WindowService::setup_keyboard() {
    if (!xkb_x11_setup_xkb_extension(connection_, XKB_X11_MIN_MAJOR_XKB_VERSION, XKB_X11_MIN_MINOR_XKB_VERSION, XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS, nullptr, nullptr, nullptr, nullptr)) {
        return false;
    }
    device_ = xkb_x11_get_core_keyboard_device_id(connection_);
    xkb_ = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    keymap_ = device_ >= 0 && xkb_ ? xkb_x11_keymap_new_from_device(xkb_, connection_, device_, XKB_KEYMAP_COMPILE_NO_FLAGS) : nullptr;
    return keymap_ && reload_state();
}

bool WindowService::reload_state() {
    xkb_state *state = xkb_x11_state_new_from_device(keymap_, connection_, device_);
    if (!state) {
        return false;
    }
    if (state_) {
        xkb_state_unref(state_);
    }
    state_ = state;
    return true;
}

int WindowService::width() const {
    return width_;
}

int WindowService::height() const {
    return height_;
}

void WindowService::present(const render::Canvas &canvas, const std::vector<render::Rect> &damage) {
    if (!surface_ || damage.empty()) {
        return;
    }
    cairo_surface_t *source = cairo_image_surface_create_for_data(const_cast<std::uint8_t *>(canvas.data()), CAIRO_FORMAT_RGB24, canvas.width(), canvas.height(), canvas.stride());
    cairo_t *cr = cairo_create(surface_);
    for (const auto &rect : damage) {
        cairo_rectangle(cr, rect.x, rect.y, rect.width, rect.height);
    }
    cairo_clip(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(cr, source, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(source);
    cairo_surface_flush(surface_);
    xcb_flush(connection_);
}

bool WindowService::caps_lock() const {
    return state_ && xkb_state_mod_name_is_active(state_, XKB_MOD_NAME_CAPS, XKB_STATE_MODS_LOCKED) > 0;
}

std::string WindowService::layout_name() const {
    if (!state_) {
        return {};
    }
    const char *name = xkb_keymap_layout_get_name(keymap_, xkb_state_serialize_layout(state_, XKB_STATE_LAYOUT_EFFECTIVE));
    return name ? name : "";
}

void WindowService::close() {
    if (surface_) {
        cairo_surface_destroy(surface_);
        surface_ = nullptr;
    }
    if (state_) {
        xkb_state_unref(state_);
        state_ = nullptr;
    }
    if (keymap_) {
        xkb_keymap_unref(keymap_);
        keymap_ = nullptr;
    }
    if (xkb_) {
        xkb_context_unref(xkb_);
        xkb_ = nullptr;
    }
    if (connection_) {
        loop_.remove_fd(xcb_get_file_descriptor(connection_));
        xcb_disconnect(connection_);
        connection_ = nullptr;
    }
}

void WindowService::dispatch() {
    while (xcb_generic_event_t *event = xcb_poll_for_event(connection_)) {
        handle(event);
        std::free(event);
    }
    if (xcb_connection_has_error(connection_)) {
        core::error("lost the X server connection");
        loop_.remove_fd(xcb_get_file_descriptor(connection_));
        if (on_close) {
            on_close();
        }
        return;
    }
    xcb_flush(connection_);
}

void WindowService::handle(xcb_generic_event_t *event) {
    switch (event->response_type & 0x7F) {
    case XCB_KEY_PRESS: {
        std::uint32_t code = reinterpret_cast<xcb_key_press_event_t *>(event)->detail;
        xkb_state_update_key(state_, code, XKB_KEY_DOWN);
        emit(code);
        break;
    }
    case XCB_KEY_RELEASE:
        xkb_state_update_key(state_, reinterpret_cast<xcb_key_release_event_t *>(event)->detail, XKB_KEY_UP);
        break;
    case XCB_FOCUS_IN:
        if (reload_state() && on_modifiers) {
            on_modifiers();
        }
        break;
    case XCB_BUTTON_PRESS: {
        auto *button = reinterpret_cast<xcb_button_press_event_t *>(event);
        if (button->detail == XCB_BUTTON_INDEX_1 && on_click) {
            on_click(button->event_x, button->event_y);
        }
        break;
    }
    case XCB_MOTION_NOTIFY: {
        auto *motion = reinterpret_cast<xcb_motion_notify_event_t *>(event);
        if (on_pointer_motion) {
            on_pointer_motion(motion->event_x, motion->event_y);
        }
        break;
    }
    case XCB_EXPOSE: {
        auto *expose = reinterpret_cast<xcb_expose_event_t *>(event);
        if (on_expose) {
            on_expose({expose->x, expose->y, expose->width, expose->height});
        }
        break;
    }
    case XCB_CONFIGURE_NOTIFY: {
        auto *configure = reinterpret_cast<xcb_configure_notify_event_t *>(event);
        if (configure->width == width_ && configure->height == height_) {
            break;
        }
        width_ = configure->width;
        height_ = configure->height;
        cairo_xcb_surface_set_size(surface_, width_, height_);
        if (on_resize) {
            on_resize(width_, height_);
        }
        break;
    }
    case XCB_CLIENT_MESSAGE: {
        auto *message = reinterpret_cast<xcb_client_message_event_t *>(event);
        if (message->data.data32[0] == delete_atom_ && on_close) {
            on_close();
        }
        break;
    }
    default:
        break;
    }
}

void WindowService::emit(std::uint32_t keycode) {
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

} // namespace service
