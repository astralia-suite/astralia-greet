#include "app/greeter.h"

#include <algorithm>
#include <csignal>
#include <cstdint>
#include <format>
#include <sys/wait.h>
#include <utility>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "app/module_registry.h"

#include "config/window_config.h"

#include "core/log.h"
#include "core/strings.h"

#include "render/text.h"

namespace app {

namespace {

int release_signal() {
    return SIGRTMIN;
}

int acquire_signal() {
    return SIGRTMIN + 1;
}

GreeterState initial_state(const config::Settings &settings, const service::UserService &users, const service::SessionService &sessions, const StateStore &store) {
    GreeterState state;
    state.users = users.users();
    state.sessions = sessions.sessions();
    state.password.reserve(256);
    if (settings.general.remember_last_user) {
        if (auto last = store.last_user()) {
            auto found = std::ranges::find(state.users, *last, &service::User::name);
            if (found != state.users.end()) {
                state.user_index = static_cast<std::size_t>(found - state.users.begin());
            }
        }
    }
    return state;
}

std::optional<std::size_t> preferred_session(const config::Settings &settings, const service::SessionService &sessions, const StateStore &store, const std::string &user) {
    if (settings.general.remember_last_session && !user.empty()) {
        if (auto last = store.last_session(user)) {
            if (auto index = sessions.index_of(*last)) {
                return index;
            }
        }
    }
    if (!settings.general.default_session.empty()) {
        return sessions.index_of(settings.general.default_session);
    }
    return std::nullopt;
}

bool is_modifier(std::uint32_t sym) {
    return (sym >= XKB_KEY_Shift_L && sym <= XKB_KEY_Hyper_R) || (sym >= XKB_KEY_ISO_Lock && sym <= XKB_KEY_ISO_Level5_Lock) || sym == XKB_KEY_Num_Lock;
}

bool is_printable(const service::KeyEvent &key) {
    return !key.ctrl && !key.alt && !key.text.empty() && static_cast<unsigned char>(key.text[0]) >= 0x20 && key.text[0] != 0x7F;
}

} // namespace

Greeter::Greeter(core::EventLoop &loop, const config::Settings &settings, Mode mode)
    : loop_(loop), settings_(settings), mode_(mode), seat_(loop), input_(loop), window_(loop), store_(config::state_path) {}

Greeter::~Greeter() {
    core::secure_clear(state_.password);
    input_.close();
    display_.close();
    seat_.close();
    vt_.close();
    window_.close();
}

bool Greeter::start() {
    store_.load();
    users_.load(settings_.users);
    sessions_.load(settings_.sessions);
    modules_ = create_modules(settings_.power);
    if (!(mode_ == Mode::Window ? start_window() : start_native())) {
        return false;
    }
    state_ = initial_state(settings_, users_, sessions_, store_);
    select_session_for_user();
    refresh_keyboard();
    active_ = true;
    damage_all();
    flush();
    for (auto &module : modules_) {
        schedule_tick(module.get());
    }
    if (mode_ == Mode::Native) {
        autologin();
    }
    return true;
}

bool Greeter::start_native() {
    loop_.add_signal(SIGCHLD, [this] { on_child_exit(); });
    if (!vt_.open(settings_.general.vt) || !vt_.activate()) {
        return false;
    }
    managed_ = seat_.open(vt_.number(), vt_.path());
    if (managed_) {
        seat_.on_enable = [this] { on_seat_enable(); };
        seat_.on_disable = [this] { deactivate(); };
    } else {
        core::warn("no logind greeter session, managing the VT and devices directly");
        if (!vt_.enter_graphics(release_signal(), acquire_signal())) {
            return false;
        }
        loop_.add_signal(release_signal(), [this] { on_vt_release(); });
        loop_.add_signal(acquire_signal(), [this] { on_vt_acquire(); });
    }
    display_.use_seat(&seat_);
    input_.use_seat(&seat_);
    if (!display_.open(settings_.general.output) || !resize(display_.width(), display_.height())) {
        return false;
    }
    input_.on_key = [this](const service::KeyEvent &key) { handle_key(key); };
    input_.on_click = [this](int x, int y) { handle_click(x, y); };
    input_.on_pointer_motion = [this](int x, int y) {
        display_.move_cursor(x, y);
        handle_motion(x, y);
    };
    input_.set_bounds(display_.width(), display_.height());
    return input_.open(settings_.general);
}

bool Greeter::start_window() {
    if (!window_.open() || !resize(window_.width(), window_.height())) {
        return false;
    }
    window_.on_key = [this](const service::KeyEvent &key) { handle_key(key); };
    window_.on_click = [this](int x, int y) { handle_click(x, y); };
    window_.on_pointer_motion = [this](int x, int y) { handle_motion(x, y); };
    window_.on_resize = [this](int width, int height) {
        if (resize(width, height)) {
            damage_all();
            flush();
        }
    };
    window_.on_expose = [this](const render::Rect &rect) {
        damage(rect);
        flush();
    };
    window_.on_modifiers = [this] {
        refresh_keyboard();
        damage_widgets();
        flush();
    };
    window_.on_close = [this] { loop_.quit(); };
    core::info("test window: logins are checked but never started, power requests are only logged, Ctrl+Q quits");
    return true;
}

bool Greeter::resize(int width, int height) {
    auto canvas = std::make_unique<render::Canvas>(width, height);
    if (!canvas->valid()) {
        core::error("cannot allocate the back buffer");
        return false;
    }
    canvas_ = std::move(canvas);
    render::set_text_scale(render::ui_scale(width, height));
    for (auto &module : modules_) {
        module->layout(width, height);
    }
    return true;
}

void Greeter::refresh_keyboard() {
    state_.caps_lock = mode_ == Mode::Window ? window_.caps_lock() : input_.caps_lock();
    state_.layout = mode_ == Mode::Window ? window_.layout_name() : input_.layout_name();
}

std::string Greeter::selected_username() const {
    if (state_.users.empty()) {
        return {};
    }
    return state_.users[state_.user_index].name;
}

void Greeter::select_session_for_user() {
    if (auto index = preferred_session(settings_, sessions_, store_, selected_username())) {
        state_.session_index = *index;
    }
    if (state_.session_index >= state_.sessions.size()) {
        state_.session_index = 0;
    }
}

void Greeter::handle_key(const service::KeyEvent &key) {
    if (!active_ || state_.busy || worker_ > 0) {
        return;
    }
    auto previous_user = state_.user_index;
    auto sym = key.keysym;
    if (sym >= XKB_KEY_XF86Switch_VT_1 && sym <= XKB_KEY_XF86Switch_VT_12) {
        int target = static_cast<int>(sym - XKB_KEY_XF86Switch_VT_1) + 1;
        if (managed_) {
            seat_.switch_to(target);
        } else if (mode_ == Mode::Native) {
            vt_.switch_to(target);
        }
        return;
    }
    if (mode_ == Mode::Window && key.ctrl && (sym == XKB_KEY_q || sym == XKB_KEY_Q)) {
        loop_.quit();
        return;
    }
    if ((state_.session_menu_open || state_.user_menu_open) && (sym == XKB_KEY_Return || sym == XKB_KEY_KP_Enter || sym == XKB_KEY_Escape)) {
        state_.session_menu_open = false;
        state_.user_menu_open = false;
        after_input(previous_user);
        return;
    }
    if (state_.view == View::Clock) {
        handle_clock_key(key);
        after_input(previous_user);
        return;
    }
    switch (sym) {
    case XKB_KEY_Return:
    case XKB_KEY_KP_Enter:
        state_.request = Request::Login;
        break;
    case XKB_KEY_Escape:
        show_clock();
        break;
    case XKB_KEY_BackSpace:
        core::utf8_pop_back(state_.password);
        state_.message.clear();
        break;
    case XKB_KEY_XF86PowerOff:
        if (settings_.power.allow_poweroff) {
            state_.request = Request::PowerOff;
        }
        break;
    default:
        if (key.ctrl && (sym == XKB_KEY_u || sym == XKB_KEY_U)) {
            core::secure_clear(state_.password);
            state_.message.clear();
        } else if (is_printable(key)) {
            state_.password += key.text;
            state_.message.clear();
        }
        break;
    }
    after_input(previous_user);
}

void Greeter::handle_clock_key(const service::KeyEvent &key) {
    switch (key.keysym) {
    case XKB_KEY_Up:
    case XKB_KEY_Down:
        state_.focus = Focus::Session;
        step(key.keysym == XKB_KEY_Up ? -1 : 1);
        break;
    case XKB_KEY_Left:
    case XKB_KEY_Right:
        state_.focus = Focus::Password;
        step(key.keysym == XKB_KEY_Left ? -1 : 1);
        break;
    case XKB_KEY_Escape:
        state_.message.clear();
        break;
    case XKB_KEY_XF86PowerOff:
        if (settings_.power.allow_poweroff) {
            state_.request = Request::PowerOff;
        }
        break;
    default:
        if (is_modifier(key.keysym)) {
            break;
        }
        show_login();
        if (is_printable(key)) {
            state_.password += key.text;
        }
        break;
    }
}

void Greeter::show_login() {
    state_.view = View::Login;
    state_.focus = Focus::Password;
    state_.message.clear();
    state_.session_menu_open = false;
    state_.user_menu_open = false;
}

void Greeter::show_clock() {
    core::secure_clear(state_.password);
    state_.message.clear();
    state_.session_menu_open = false;
    state_.user_menu_open = false;
    state_.focus = Focus::Password;
    state_.view = View::Clock;
}

void Greeter::handle_click(int x, int y) {
    if (!active_ || state_.busy || worker_ > 0) {
        return;
    }
    auto previous_user = state_.user_index;
    bool session_menu_was_open = state_.session_menu_open;
    bool user_menu_was_open = state_.user_menu_open;
    damage_widgets(); // pre-click bounds: click() may shrink them before after_input() runs
    for (auto it = modules_.rbegin(); it != modules_.rend(); ++it) {
        auto &module = *it;
        if (!module->is_backdrop() && module->bounds().contains(x, y) && module->click(x, y, state_)) {
            break;
        }
    }
    if (session_menu_was_open) {
        state_.session_menu_open = false;
    }
    if (user_menu_was_open) {
        state_.user_menu_open = false;
    }
    after_input(previous_user);
}

void Greeter::handle_motion(int x, int y) {
    if (!active_ || worker_ > 0) {
        return;
    }
    for (auto &module : modules_) {
        if (module->hover(x, y, state_)) {
            damage(module->bounds());
        }
    }
    flush();
}

void Greeter::after_input(std::size_t previous_user) {
    if (state_.user_index != previous_user) {
        core::secure_clear(state_.password);
        state_.message.clear();
        select_session_for_user();
    }
    refresh_keyboard();
    if (state_.view != shown_view_) {
        shown_view_ = state_.view;
        damage_all();
    }
    damage_widgets();
    flush();
    process_request();
    flush();
}

void Greeter::step(int direction) {
    auto advance = [direction](std::size_t index, std::size_t count) {
        return count == 0 ? 0 : (index + static_cast<std::size_t>(static_cast<int>(count) + direction)) % count;
    };
    if (state_.focus == Focus::Session) {
        state_.session_index = advance(state_.session_index, state_.sessions.size());
    } else {
        state_.user_index = advance(state_.user_index, state_.users.size());
    }
}

void Greeter::process_request() {
    auto request = std::exchange(state_.request, Request::None);
    if (mode_ == Mode::Window && request != Request::None && request != Request::Login) {
        core::info("test window: ignoring power request");
        return;
    }
    switch (request) {
    case Request::Login:
        login();
        break;
    case Request::Reboot:
        power_.reboot();
        break;
    case Request::PowerOff:
        power_.power_off();
        break;
    case Request::Suspend:
        power_.suspend();
        break;
    case Request::None:
        break;
    }
}

void Greeter::show_error(std::string message) {
    core::warn("{}", message);
    state_.message = std::move(message);
    state_.message_is_error = true;
    state_.busy = false;
    damage_widgets();
    flush();
}

void Greeter::login() {
    auto name = selected_username();
    if (name.empty()) {
        show_error("No user to log in");
        return;
    }
    auto user = users_.find(name);
    if (!user) {
        core::secure_clear(state_.password);
        show_error("Unknown user");
        return;
    }
    if (state_.sessions.empty()) {
        show_error("No sessions found");
        return;
    }
    auto session = state_.sessions[state_.session_index];
    state_.busy = true;
    state_.message.clear();
    damage_widgets();
    flush();
    if (!pick_session_vt()) {
        core::secure_clear(state_.password);
        show_error("No free VT");
        return;
    }
    auto auth = std::make_unique<service::AuthService>();
    if (!auth->start(config::pam_service, user->name, session_tty())) {
        core::secure_clear(state_.password);
        show_error("Authentication is unavailable");
        return;
    }
    auto result = auth->authenticate(state_.password);
    core::secure_clear(state_.password);
    if (!result) {
        core::warn("authentication failed for {}", user->name);
        show_error(result.error());
        return;
    }
    state_.busy = false;
    start_session(std::move(auth), *user, session);
}

void Greeter::autologin() {
    const auto &autologin = settings_.autologin;
    if (autologin.user.empty()) {
        return;
    }
    auto user = users_.find(autologin.user);
    if (!user) {
        core::warn("autologin user {} does not exist", autologin.user);
        return;
    }
    auto index = sessions_.index_of(autologin.session);
    if (!index) {
        index = preferred_session(settings_, sessions_, store_, user->name);
    }
    if (!index && !state_.sessions.empty()) {
        index = 0;
    }
    if (!index) {
        show_error("No sessions found");
        return;
    }
    if (!pick_session_vt()) {
        show_error("No free VT");
        return;
    }
    auto auth = std::make_unique<service::AuthService>();
    if (!auth->start(config::pam_autologin_service, user->name, session_tty())) {
        return;
    }
    if (auto result = auth->authenticate(""); !result) {
        show_error(result.error());
        return;
    }
    start_session(std::move(auth), *user, state_.sessions[*index]);
}

void Greeter::start_session(std::unique_ptr<service::AuthService> auth, const service::User &user, const service::Session &session) {
    if (mode_ == Mode::Window) {
        auth->end(false);
        core::info("test window: {} would start {}", user.name, session.id);
        state_.message = config::window::test_login_message;
        state_.message_is_error = false;
        damage_widgets();
        flush();
        return;
    }
    if (settings_.general.remember_last_user) {
        store_.set_last_user(user.name);
    }
    if (settings_.general.remember_last_session) {
        store_.set_last_session(user.name, session.id);
    }
    if (!store_.save()) {
        core::warn("cannot save state to {}", config::state_path);
    }
    deactivate();
    input_.close();
    if (!managed_) {
        vt_.leave_graphics();
    }
    service::LaunchRequest request{user, session, session_vt_, settings_, managed_};
    auto pid = launch_.launch(*auth, request);
    auth->end(pid.has_value());
    if (!pid) {
        if (managed_ && !seat_.enabled()) {
            seat_.switch_to(vt_.number());
        } else {
            if (!managed_) {
                vt_.enter_graphics(release_signal(), acquire_signal());
            }
            activate();
        }
        show_error("Failed to start the session");
        return;
    }
    worker_ = *pid;
    session_started_ = std::chrono::steady_clock::now();
    core::info("started {} for {}", session.id, user.name);
}

void Greeter::on_child_exit() {
    int status = 0;
    pid_t pid = 0;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (pid == worker_) {
            worker_ = 0;
            on_session_end();
        } else if (managed_ && pid == seat_.holder()) {
            core::error("the greeter logind session ended, restarting");
            loop_.quit();
        }
    }
}

void Greeter::on_session_end() {
    auto lasted = std::chrono::steady_clock::now() - session_started_;
    core::info("session ended");
    if (!managed_) {
        vt_.activate();
        vt_.enter_graphics(release_signal(), acquire_signal());
    }
    core::secure_clear(state_.password);
    state_.message.clear();
    state_.busy = false;
    state_.session_menu_open = false;
    state_.user_menu_open = false;
    state_.focus = Focus::Password;
    state_.view = View::Clock;
    shown_view_ = View::Clock;
    if (managed_ && !seat_.enabled()) {
        seat_.switch_to(vt_.number());
    } else {
        activate();
    }
    if (settings_.autologin.relogin && lasted > std::chrono::seconds(config::relogin_min_session_seconds)) {
        autologin();
    }
}

void Greeter::on_vt_release() {
    if (worker_ > 0) {
        return;
    }
    deactivate();
    vt_.ack_release();
}

void Greeter::on_vt_acquire() {
    if (worker_ > 0) {
        return;
    }
    vt_.ack_acquire();
    activate();
}

void Greeter::on_seat_enable() {
    if (worker_ > 0) {
        seat_.switch_to(session_vt_);
        return;
    }
    activate();
}

bool Greeter::pick_session_vt() {
    if (mode_ == Mode::Window) {
        session_vt_ = 0;
        return true;
    }
    session_vt_ = managed_ ? service::VtService::find_free() : vt_.number();
    return session_vt_ > 0;
}

std::string Greeter::session_tty() const {
    return std::format("/dev/tty{}", session_vt_);
}

void Greeter::activate() {
    if (active_ || worker_ > 0) {
        return;
    }
    display_.acquire_master();
    if (!input_.open(settings_.general)) {
        core::error("cannot open input");
    }
    input_.resume();
    active_ = true;
    refresh_keyboard();
    damage_all();
    flush();
}

void Greeter::deactivate() {
    if (!active_) {
        return;
    }
    active_ = false;
    state_.session_menu_open = false;
    state_.user_menu_open = false;
    input_.suspend();
    display_.drop_master();
}

void Greeter::schedule_tick(Module *module) {
    auto delay = module->next_tick();
    if (!delay) {
        return;
    }
    loop_.add_timer(*delay, std::chrono::milliseconds{0}, [this, module] {
        if (module->visible(state_)) {
            damage(module->bounds());
            flush();
        }
        schedule_tick(module);
    });
}

void Greeter::damage(const render::Rect &rect) {
    damage_.push_back(rect.padded(config::damage_padding));
}

void Greeter::damage_widgets() {
    for (auto &module : modules_) {
        if (!module->is_backdrop() && module->visible(state_)) {
            damage(module->bounds());
        }
    }
}

void Greeter::damage_all() {
    damage_.clear();
    damage_.push_back({0, 0, canvas_->width(), canvas_->height()});
}

void Greeter::flush() {
    if (!active_ || !canvas_) {
        damage_.clear();
        return;
    }
    if (damage_.empty()) {
        return;
    }
    cairo_t *cr = canvas_->context();
    cairo_save(cr);
    cairo_new_path(cr);
    for (const auto &rect : damage_) {
        cairo_rectangle(cr, rect.x, rect.y, rect.width, rect.height);
    }
    cairo_clip(cr);
    for (auto &module : modules_) {
        auto bounds = module->bounds().padded(config::damage_padding);
        bool hit = module->is_backdrop() || std::ranges::any_of(damage_, [&](const render::Rect &rect) { return rect.intersects(bounds); });
        if (hit) {
            cairo_save(cr);
            module->draw(cr, state_);
            cairo_restore(cr);
        }
    }
    cairo_restore(cr);
    cairo_surface_flush(cairo_get_target(cr));
    if (mode_ == Mode::Window) {
        window_.present(*canvas_, damage_);
    } else {
        display_.present(*canvas_, damage_);
    }
    damage_.clear();
}

bool render_preview(const config::Settings &settings, const std::filesystem::path &output, int width, int height) {
    render::Canvas canvas(width, height);
    if (!canvas.valid()) {
        return false;
    }
    service::UserService users;
    users.load(settings.users);
    service::SessionService sessions;
    sessions.load(settings.sessions);
    StateStore store(config::state_path);
    store.load();
    auto state = initial_state(settings, users, sessions, store);
    state.password = "preview";
    auto modules = create_modules(settings.power);
    render::set_text_scale(render::ui_scale(width, height));
    for (auto &module : modules) {
        module->layout(width, height);
        cairo_save(canvas.context());
        module->draw(canvas.context(), state);
        cairo_restore(canvas.context());
    }
    return canvas.write_png(output);
}

} // namespace app
