#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <sys/types.h>
#include <vector>

#include "app/greeter_state.h"
#include "app/module.h"
#include "app/state_store.h"

#include "config/greet_config.h"

#include "core/event_loop.h"

#include "render/canvas.h"
#include "render/layout.h"

#include "service/auth_service.h"
#include "service/display_service.h"
#include "service/input_service.h"
#include "service/launch_service.h"
#include "service/power_service.h"
#include "service/seat_service.h"
#include "service/session_service.h"
#include "service/user_service.h"
#include "service/vt_service.h"
#include "service/window_service.h"

namespace app {

class Greeter {
  public:
    enum class Mode { Native,
                      Window };

    Greeter(core::EventLoop &loop, const config::Settings &settings, Mode mode = Mode::Native);
    ~Greeter();
    Greeter(const Greeter &) = delete;
    Greeter &operator=(const Greeter &) = delete;

    bool start();

  private:
    bool start_native();
    bool start_window();
    bool resize(int width, int height);
    void refresh_keyboard();
    void select_session_for_user();
    std::string selected_username() const;
    void handle_key(const service::KeyEvent &key);
    void handle_clock_key(const service::KeyEvent &key);
    void show_login();
    void show_clock();
    void handle_click(int x, int y);
    void handle_motion(int x, int y);
    void step(int direction);
    void after_input(std::size_t previous_user);
    void process_request();
    void login();
    void autologin();
    void start_session(std::unique_ptr<service::AuthService> auth, const service::User &user, const service::Session &session);
    void on_child_exit();
    void on_session_end();
    void on_vt_release();
    void on_vt_acquire();
    void on_seat_enable();
    bool pick_session_vt();
    std::string session_tty() const;
    void activate();
    void deactivate();
    void schedule_tick(Module *module);
    void damage(const render::Rect &rect);
    void damage_widgets();
    void damage_all();
    void flush();
    void show_error(std::string message);

    core::EventLoop &loop_;
    const config::Settings &settings_;
    Mode mode_;
    service::VtService vt_;
    service::SeatService seat_;
    service::DisplayService display_;
    service::InputService input_;
    service::WindowService window_;
    service::UserService users_;
    service::SessionService sessions_;
    service::PowerService power_;
    service::LaunchService launch_;
    StateStore store_;
    std::unique_ptr<render::Canvas> canvas_;
    std::vector<std::unique_ptr<Module>> modules_;
    GreeterState state_;
    std::vector<render::Rect> damage_;
    View shown_view_ = View::Clock;
    bool active_ = false;
    bool managed_ = false;
    int session_vt_ = 0;
    pid_t worker_ = 0;
    std::chrono::steady_clock::time_point session_started_;
};

bool render_preview(const config::Settings &settings, const std::filesystem::path &output, int width, int height);

} // namespace app
